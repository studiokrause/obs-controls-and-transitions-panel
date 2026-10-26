/*
 * Horizontal Scene Transitions - the OBS "Scene Transitions" dock in one row:
 * transition list, add / remove / properties buttons and duration.
 */
#include "hpanels.hpp"
#include "plugin-version.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

using namespace hpanels;

namespace {

QStringList TransitionNames(QList<obs_source_t *> *sources = nullptr)
{
	QStringList names;
	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		names << QString::fromUtf8(obs_source_get_name(source));
		if (sources)
			sources->append(obs_source_get_ref(source));
	}
	obs_frontend_source_list_free(&list);
	return names;
}

class HorizontalSceneTransitions : public HorizontalPanel {
public:
	HorizontalSceneTransitions()
	{
		setObjectName("horizontalSceneTransitionsPanel");

		ownModel = new QStandardItemModel(this);

		combo = new QComboBox(this);
		combo->setObjectName("hst_transitions");
		combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		combo->setMinimumWidth(100);
		combo->setModel(ownModel);
		row->addWidget(combo, 1);

		addButton = new MirrorButton(this, MirrorButton::Width::Compact);
		removeButton = new MirrorButton(this, MirrorButton::Width::Compact);
		propsButton = new MirrorButton(this, MirrorButton::Width::Compact);
		addButton->setObjectName("hst_transitionAdd");
		removeButton->setObjectName("hst_transitionRemove");
		propsButton->setObjectName("hst_transitionProps");
		row->addWidget(addButton);
		row->addWidget(removeButton);
		row->addWidget(propsButton);

		row->addWidget(MakeSeparator(this));

		durationLabel = new QLabel(this);
		durationLabel->setObjectName("hst_transitionDurationLabel");
		durationLabel->setText(ObsText("Basic.TransitionDuration"));
		row->addWidget(durationLabel);

		duration = new QSpinBox(this);
		duration->setObjectName("hst_transitionDuration");
		duration->setRange(50, 20000);
		duration->setSingleStep(50);
		duration->setSuffix(QStringLiteral(" ms"));
		duration->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		row->addWidget(duration);

		/* activated / valueChanged fire for user input only: every update
		 * coming from OBS is applied under a QSignalBlocker */
		connect(combo, &QComboBox::activated, this, [this](int index) {
			if (origCombo)
				origCombo->setCurrentIndex(index);
			else
				SetTransitionFallback(index);
		});
		connect(duration, &QSpinBox::valueChanged, this, [this](int value) {
			if (origDuration)
				origDuration->setValue(value);
			else
				obs_frontend_set_transition_duration(value);
		});

		propsButton->fallbackClick = []() {
			obs_source_t *tr = obs_frontend_get_current_transition();
			if (tr && obs_source_configurable(tr))
				obs_frontend_open_source_properties(tr);
			obs_source_release(tr);
		};
		propsButton->fallbackSync = [](MirrorButton *b) {
			obs_source_t *tr = obs_frontend_get_current_transition();
			b->setEnabled(tr && obs_source_configurable(tr));
			obs_source_release(tr);
			b->setProperty("class", "icon-dots-vert");
			b->setVisible(true);
		};
	}

protected:
	void Tick() override
	{
		if (Exiting())
			return;

		bool missing = !origCombo || !origDuration || !addButton->Mirroring() || !removeButton->Mirroring() ||
			       !propsButton->Mirroring() || !origLabel;
		if (missing && ShouldSearch())
			Locate();

		SyncCombo();
		SyncDuration();
		addButton->Sync();
		removeButton->Sync();
		propsButton->Sync();

		if (origLabel && durationLabel->text() != origLabel->text())
			durationLabel->setText(origLabel->text());
	}

private:
	void Locate()
	{
		if (!origCombo)
			origCombo = FindOriginal<QComboBox>("transitions");
		if (!origDuration)
			origDuration = FindOriginal<QSpinBox>("transitionDuration");
		if (!origLabel)
			origLabel = FindOriginal<QLabel>("transitionDurationLabel");
		if (!addButton->Mirroring())
			addButton->SetTarget(FindOriginal<QAbstractButton>("transitionAdd"));
		if (!removeButton->Mirroring())
			removeButton->SetTarget(FindOriginal<QAbstractButton>("transitionRemove"));
		if (!propsButton->Mirroring())
			propsButton->SetTarget(FindOriginal<QAbstractButton>("transitionProps"));

		int found = !!origCombo + !!origDuration + !!origLabel + addButton->Mirroring() +
			    removeButton->Mirroring() + propsButton->Mirroring();
		if (found != lastFound) {
			blog(LOG_INFO, "[%s] mirroring %d of 6 stock scene transition widgets", PLUGIN_NAME, found);
			lastFound = found;
		}
	}

	void SyncCombo()
	{
		QSignalBlocker block(combo);

		if (QComboBox *orig = origCombo) {
			/* Share the stock model: names, order and renames stay
			 * identical without any copying */
			if (combo->model() != orig->model())
				combo->setModel(orig->model());
			if (combo->currentIndex() != orig->currentIndex())
				combo->setCurrentIndex(orig->currentIndex());
			if (combo->isEnabled() != orig->isEnabled())
				combo->setEnabled(orig->isEnabled());
			return;
		}

		if (combo->model() != ownModel)
			combo->setModel(ownModel);

		QStringList names = TransitionNames();
		QStringList current;
		for (int i = 0; i < ownModel->rowCount(); i++)
			current << ownModel->item(i)->text();
		if (names != current) {
			ownModel->clear();
			for (const QString &name : names)
				ownModel->appendRow(new QStandardItem(name));
		}

		obs_source_t *tr = obs_frontend_get_current_transition();
		int index = tr ? names.indexOf(QString::fromUtf8(obs_source_get_name(tr))) : -1;
		obs_source_release(tr);
		if (combo->currentIndex() != index)
			combo->setCurrentIndex(index);
		combo->setEnabled(true);
	}

	void SetTransitionFallback(int index)
	{
		QList<obs_source_t *> sources;
		TransitionNames(&sources);
		if (index >= 0 && index < sources.size())
			obs_frontend_set_current_transition(sources[index]);
		for (obs_source_t *s : sources)
			obs_source_release(s);
	}

	void SyncDuration()
	{
		QSignalBlocker block(duration);

		if (QSpinBox *orig = origDuration) {
			if (duration->minimum() != orig->minimum() || duration->maximum() != orig->maximum())
				duration->setRange(orig->minimum(), orig->maximum());
			if (duration->singleStep() != orig->singleStep())
				duration->setSingleStep(orig->singleStep());
			if (duration->suffix() != orig->suffix())
				duration->setSuffix(orig->suffix());
			/* Don't fight the user while they are typing */
			if (!duration->hasFocus() && duration->value() != orig->value())
				duration->setValue(orig->value());
			if (duration->isEnabled() != orig->isEnabled())
				duration->setEnabled(orig->isEnabled());
			/* Keep the full value and suffix readable */
			if (duration->minimumWidth() != duration->sizeHint().width())
				duration->setMinimumWidth(duration->sizeHint().width());
			duration->setVisible(!orig->isHidden());
			durationLabel->setVisible(!orig->isHidden());
			return;
		}

		if (!duration->hasFocus()) {
			int value = obs_frontend_get_transition_duration();
			if (duration->value() != value)
				duration->setValue(value);
		}
		duration->setVisible(true);
		durationLabel->setVisible(true);
	}

	QStandardItemModel *ownModel = nullptr;
	QComboBox *combo = nullptr;
	MirrorButton *addButton = nullptr;
	MirrorButton *removeButton = nullptr;
	MirrorButton *propsButton = nullptr;
	QLabel *durationLabel = nullptr;
	QSpinBox *duration = nullptr;

	QPointer<QComboBox> origCombo;
	QPointer<QSpinBox> origDuration;
	QPointer<QLabel> origLabel;
	int lastFound = -1;
};

} // namespace

bool obs_module_load(void)
{
	blog(LOG_INFO, "[%s] version %s loaded", PLUGIN_NAME, PLUGIN_VERSION);
	return HorizontalPanel::Register(new HorizontalSceneTransitions(), "horizontal_scene_transitions_dock",
					 obs_module_text("DockTitle"));
}
