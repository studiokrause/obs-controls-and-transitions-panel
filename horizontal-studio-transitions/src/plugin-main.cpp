/*
 * Horizontal Studio Mode Transitions - the Studio Mode transition panel (the
 * column between Preview and Program) in one row: Transition button, options
 * menu, quick transitions and the T-bar.
 */
#include "hpanels.hpp"
#include "plugin-version.h"

#include <QSignalBlocker>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QWheelEvent>

#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

using namespace hpanels;

namespace {

constexpr int kTBarMax = 1023; /* T_BAR_PRECISION - 1 in OBS */

/* Like OBS' T-bar: only the handle can be dragged, clicks on the groove and
 * the mouse wheel are ignored (they would leave a transition half-way). */
class TBar : public QSlider {
public:
	explicit TBar(QWidget *parent) : QSlider(Qt::Horizontal, parent)
	{
		setRange(0, kTBarMax);
		setFocusPolicy(Qt::NoFocus);
		setProperty("class", "slider-tbar");
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setMinimumWidth(120);
	}

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		QStyleOptionSlider opt;
		initStyleOption(&opt);
		QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
		if (!handle.contains(event->position().toPoint())) {
			event->ignore();
			return;
		}
		QSlider::mousePressEvent(event);
	}
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

bool IsQuickTransitionButton(QWidget *w)
{
	return qobject_cast<QPushButton *>(w) && w->property("id").isValid();
}

class HorizontalStudioTransitions : public HorizontalPanel {
public:
	HorizontalStudioTransitions()
	{
		setObjectName("horizontalStudioTransitionsPanel");

		/* Shown while Studio Mode is off */
		offLabel = new QLabel(obs_module_text("StudioModeOff"), this);
		row->addWidget(offLabel);
		enableButton = new QPushButton(obs_module_text("EnableStudioMode"), this);
		connect(enableButton, &QPushButton::clicked, this,
			[]() { obs_frontend_set_preview_program_mode(true); });
		row->addWidget(enableButton);
		offStretch = new QWidget(this);
		offStretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		row->addWidget(offStretch);

		/* The panel itself */
		transitionButton = new MirrorButton(this, MirrorButton::Width::Text);
		transitionButton->setObjectName("hstm_transitionButton");
		const QColor blue(0x1e, 0x6f, 0xd9);
		transitionButton->SetColors(blue, blue);
		transitionButton->fallbackClick = []() { obs_frontend_preview_program_trigger_transition(); };
		transitionButton->fallbackSync = [](MirrorButton *b) {
			b->setText(ObsText("Transition"));
			b->setVisible(true);
		};
		row->addWidget(transitionButton, 2);

		configButton = new MirrorButton(this, MirrorButton::Width::Compact);
		configButton->setObjectName("hstm_configTransitions");
		row->addWidget(configButton);

		separator1 = MakeSeparator(this);
		row->addWidget(separator1);

		quickLabel = new QLabel(this);
		row->addWidget(quickLabel);

		/* Quick transition buttons are inserted here, before "+" */
		quickInsertIndex = row->count();

		addQuickButton = new MirrorButton(this, MirrorButton::Width::Compact);
		addQuickButton->setObjectName("hstm_addQuickTransition");
		row->addWidget(addQuickButton);

		separator2 = MakeSeparator(this);
		row->addWidget(separator2);

		tBar = new TBar(this);
		tBar->setObjectName("hstm_tBar");
		row->addWidget(tBar, 3);

		connect(tBar, &QSlider::valueChanged, this, [this](int value) {
			if (QSlider *orig = origTBar)
				orig->setValue(value); /* runs OBS' TBarChanged */
			else
				obs_frontend_set_tbar_position(value);
		});
		connect(tBar, &QSlider::sliderReleased, this, [this]() {
			if (QSlider *orig = origTBar)
				emit orig->sliderReleased();
			else
				obs_frontend_release_tbar();
		});

		UpdateMode();
	}

protected:
	void FrontendEvent(enum obs_frontend_event event) override
	{
		if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
		    event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
			UpdateMode();
	}

	void Tick() override
	{
		if (Exiting())
			return;

		bool studio = obs_frontend_preview_program_mode_active();
		if (studio != studioMode)
			UpdateMode();
		if (!studio)
			return;

		if (!origPanel && ShouldSearch())
			Locate();

		QWidget *panel = origPanel;

		if (panel) {
			if (!transitionButton->Mirroring() || !configButton->Mirroring() || !addQuickButton->Mirroring() ||
			    !origQuickLabel)
				LocateChildren(panel);
		}

		transitionButton->Sync();
		configButton->Sync();
		addQuickButton->Sync();
		SyncQuickTransitions(panel);

		if (QLabel *label = origQuickLabel) {
			if (quickLabel->text() != label->text())
				quickLabel->setText(label->text());
			quickLabel->setVisible(!label->isHidden());
		} else {
			quickLabel->setVisible(false);
		}
		bool hasQuick = quickLabel->isVisibleTo(this) || addQuickButton->isVisibleTo(this) ||
				!quickButtons.empty();
		separator1->setVisible(hasQuick);

		SyncTBar();
	}

private:
	void UpdateMode()
	{
		studioMode = obs_frontend_preview_program_mode_active();
		offLabel->setVisible(!studioMode);
		enableButton->setVisible(!studioMode);
		offStretch->setVisible(!studioMode);

		transitionButton->setVisible(studioMode);
		separator2->setVisible(studioMode);
		tBar->setVisible(studioMode);
		if (!studioMode) {
			configButton->hide();
			quickLabel->hide();
			addQuickButton->hide();
			separator1->hide();
			for (MirrorButton *b : quickButtons)
				b->hide();
		}
		/* OBS rebuilds the panel every time Studio Mode is enabled */
		SearchNow();
	}

	/* The panel is located through its T-bar, the only horizontal QSlider
	 * in OBS with the T-bar range. This also finds the panel when another
	 * plugin (e.g. obs-panel-mover) has moved it into a dock. */
	void Locate()
	{
		QMainWindow *main = MainWindow();
		if (!main)
			return;

		QSlider *found = nullptr;
		for (QSlider *slider : main->findChildren<QSlider *>()) {
			if (IsOurs(slider) || slider->orientation() != Qt::Horizontal)
				continue;
			if (HasClass(slider, "slider-tbar") ||
			    (slider->minimum() == 0 && slider->maximum() == kTBarMax)) {
				found = slider;
				break;
			}
		}

		origTBar = found;
		origPanel = found ? found->parentWidget() : nullptr;
		origQuickLabel = nullptr;
		transitionButton->SetTarget(nullptr);
		configButton->SetTarget(nullptr);
		addQuickButton->SetTarget(nullptr);

		if (found != loggedTBar) {
			blog(LOG_INFO, "[%s] studio mode transition panel %s", PLUGIN_NAME,
			     found ? "found" : "not found, using frontend API");
			loggedTBar = found;
		}
	}

	void LocateChildren(QWidget *panel)
	{
		const QString transitionText = ObsText("Transition");
		QAbstractButton *transition = nullptr, *config = nullptr, *addQuick = nullptr;

		const auto children = panel->findChildren<QPushButton *>(Qt::FindDirectChildrenOnly);
		for (QPushButton *b : children) {
			if (IsQuickTransitionButton(b))
				continue;
			if (HasClass(b, "icon-dots-vert"))
				config = b;
			else if (HasClass(b, "icon-plus"))
				addQuick = b;
			else if (!transition && b->text() == transitionText)
				transition = b;
		}
		/* Unknown text (e.g. language switched): the expanding button */
		if (!transition) {
			for (QPushButton *b : children) {
				if (b != config && b != addQuick && !IsQuickTransitionButton(b) &&
				    b->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding) {
					transition = b;
					break;
				}
			}
		}

		transitionButton->SetTarget(transition);
		configButton->SetTarget(config);
		addQuickButton->SetTarget(addQuick);

		const auto labels = panel->findChildren<QLabel *>(Qt::FindDirectChildrenOnly);
		origQuickLabel = labels.isEmpty() ? nullptr : labels.first();
	}

	void SyncQuickTransitions(QWidget *panel)
	{
		/* Stock quick transition buttons, in their layout order */
		std::vector<QPushButton *> originals;
		if (panel && panel->layout()) {
			QLayout *layout = panel->layout();
			for (int i = 0; i < layout->count(); i++) {
				QWidget *w = layout->itemAt(i)->widget();
				if (w && IsQuickTransitionButton(w))
					originals.push_back(static_cast<QPushButton *>(w));
			}
		}

		while (quickButtons.size() < originals.size()) {
			auto b = new MirrorButton(this, MirrorButton::Width::Text);
			b->setObjectName("hstm_quickTransition");
			b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
			row->insertWidget(quickInsertIndex + (int)quickButtons.size(), b);
			quickButtons.push_back(b);
		}
		while (quickButtons.size() > originals.size()) {
			delete quickButtons.back();
			quickButtons.pop_back();
		}

		for (size_t i = 0; i < originals.size(); i++) {
			quickButtons[i]->SetTarget(originals[i]);
			quickButtons[i]->Sync();
		}
	}

	void SyncTBar()
	{
		if (tBar->isSliderDown())
			return;

		QSignalBlocker block(tBar);
		if (QSlider *orig = origTBar) {
			if (tBar->value() != orig->value())
				tBar->setValue(orig->value());
			if (tBar->isEnabled() != orig->isEnabled())
				tBar->setEnabled(orig->isEnabled());
			tBar->setVisible(!orig->isHidden());
		} else {
			int value = obs_frontend_get_tbar_position();
			if (tBar->value() != value)
				tBar->setValue(value);
			tBar->setVisible(true);
		}
		separator2->setVisible(tBar->isVisibleTo(this));
	}

	bool studioMode = false;

	QLabel *offLabel = nullptr;
	QPushButton *enableButton = nullptr;
	QWidget *offStretch = nullptr;

	MirrorButton *transitionButton = nullptr;
	MirrorButton *configButton = nullptr;
	QFrame *separator1 = nullptr;
	QLabel *quickLabel = nullptr;
	int quickInsertIndex = 0;
	std::vector<MirrorButton *> quickButtons;
	MirrorButton *addQuickButton = nullptr;
	QFrame *separator2 = nullptr;
	TBar *tBar = nullptr;

	QPointer<QWidget> origPanel;
	QPointer<QSlider> origTBar;
	QPointer<QLabel> origQuickLabel;
	QSlider *loggedTBar = nullptr;
};

} // namespace

bool obs_module_load(void)
{
	blog(LOG_INFO, "[%s] version %s loaded", PLUGIN_NAME, PLUGIN_VERSION);
	return HorizontalPanel::Register(new HorizontalStudioTransitions(), "horizontal_studio_transitions_dock",
					 obs_module_text("DockTitle"));
}
