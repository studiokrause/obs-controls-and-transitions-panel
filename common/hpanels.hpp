/*
 * Shared helpers for the horizontal OBS panels.
 *
 * Every panel in this suite is a *mirror* of a stock OBS panel: its widgets
 * copy the state (text, enabled, visibility, theme classes, menus) of the
 * original widgets and forward user input to them. OBS therefore keeps doing
 * all the real work (confirmation dialogs, output setup, hotkeys, YouTube
 * broadcast flow, ...), and the panels keep working when OBS changes its
 * internals - when an original widget cannot be found, a panel falls back to
 * the public obs-frontend-api.
 *
 * The header is compiled separately into each plugin; nothing here uses
 * Q_OBJECT, so no moc step is needed and the plugins never share symbols.
 */
#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "plugin-version.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QDockWidget>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

#include <functional>

namespace hpanels {

/* Marks a widget tree as belonging to one of our panels */
constexpr const char *kOwnedProperty = "hpanelsMirror";

inline QMainWindow *MainWindow()
{
	return static_cast<QMainWindow *>(obs_frontend_get_main_window());
}

/* String from OBS' own translations, so labels match the user's OBS language */
inline QString ObsText(const char *key)
{
	const char *text = obs_frontend_get_locale_string(key);
	return QString::fromUtf8(text && *text ? text : key);
}

inline void Repolish(QWidget *w)
{
	w->style()->unpolish(w);
	w->style()->polish(w);
	w->update();
}

inline bool IsOurs(const QWidget *w)
{
	for (; w; w = w->parentWidget()) {
		if (w->property(kOwnedProperty).toBool())
			return true;
	}
	return false;
}

/* First widget of type T with the given object name that is not ours */
template<class T> T *FindOriginal(const char *name)
{
	QMainWindow *main = MainWindow();
	if (!main)
		return nullptr;
	for (T *w : main->findChildren<T *>(QString::fromLatin1(name))) {
		if (!IsOurs(w))
			return w;
	}
	return nullptr;
}

inline bool HasClass(const QWidget *w, const char *cls)
{
	return w->property("class").toString().split(' ', Qt::SkipEmptyParts).contains(QString::fromLatin1(cls));
}

/* Copies a dynamic property used by OBS themes; returns true if it changed */
inline bool CopyProperty(const QObject *from, QObject *to, const char *name)
{
	QVariant value = from->property(name);
	if (to->property(name) == value)
		return false;
	to->setProperty(name, value);
	return true;
}

inline QFrame *MakeSeparator(QWidget *parent)
{
	auto line = new QFrame(parent);
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	return line;
}

/* ------------------------------------------------------------------------ */

/* A push button that mirrors another button. */
class MirrorButton : public QPushButton {
public:
	enum class Width { Text, Compact };

	explicit MirrorButton(QWidget *parent, Width width = Width::Text) : QPushButton(parent), widthMode(width)
	{
		if (widthMode == Width::Text) {
			/* Never narrower than the text, extra row space is
			 * shared between the buttons */
			setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
		}
		connect(this, &QPushButton::clicked, this, [this]() { Forward(); });
		hide();
	}

	void SetTarget(QAbstractButton *t)
	{
		if (target == t)
			return;
		target = t;
		if (!t)
			setMenu(nullptr);
	}
	QAbstractButton *Target() const { return target; }
	bool Mirroring() const { return !target.isNull(); }

	/* Used while the original button is missing */
	std::function<void()> fallbackClick;
	std::function<void(MirrorButton *)> fallbackSync;

	void Sync()
	{
		QAbstractButton *t = target;
		if (!t) {
			if (fallbackSync)
				fallbackSync(this);
			else
				hide();
			return;
		}

		if (text() != t->text())
			setText(t->text());
		QString tip = t->toolTip();
		if (tip.isEmpty() && widthMode == Width::Text)
			tip = t->text(); /* the text may be clipped in a narrow row */
		if (toolTip() != tip)
			setToolTip(tip);
		if (accessibleName() != t->accessibleName())
			setAccessibleName(t->accessibleName());
		if (isEnabled() != t->isEnabled())
			setEnabled(t->isEnabled());
		if (isCheckable() != t->isCheckable())
			setCheckable(t->isCheckable());
		if (isChecked() != t->isChecked())
			setChecked(t->isChecked());

		if (widthMode == Width::Compact) {
			if (minimumSize() != t->minimumSize())
				setMinimumSize(t->minimumSize());
			if (maximumSize() != t->maximumSize())
				setMaximumSize(t->maximumSize());
		}

		bool restyle = CopyProperty(t, this, "class");
		restyle |= CopyProperty(t, this, "themeID");
		restyle |= CopyProperty(t, this, "broadcastState");
		if (restyle)
			Repolish(this);

		auto pb = qobject_cast<QPushButton *>(t);
		QMenu *m = pb ? pb->menu() : nullptr;
		split = m && t->inherits("MenuButton");
		if (menu() != m)
			setMenu(m);

		/* isHidden() rather than isVisible(): the original stays logically
		 * visible when the user closes the stock dock */
		if (isHidden() != t->isHidden())
			setVisible(!t->isHidden());
	}

protected:
	/* For a split "MenuButton" (quick transitions) only the right edge opens
	 * the menu, the rest of the button acts as a normal click - like OBS. */
	void mousePressEvent(QMouseEvent *event) override
	{
		if (split && menu()) {
			if (width() - event->position().x() <= 30)
				showMenu();
			else
				setDown(true);
			return;
		}
		QPushButton::mousePressEvent(event);
	}

	void keyPressEvent(QKeyEvent *event) override
	{
		if (split && menu()) {
			switch (event->key()) {
			case Qt::Key_Enter:
			case Qt::Key_Return:
				Forward();
				return;
			case Qt::Key_Down:
			case Qt::Key_Space:
				showMenu();
				return;
			}
		}
		QPushButton::keyPressEvent(event);
	}

private:
	void Forward()
	{
		if (QAbstractButton *t = target) {
			if (split) {
				/* click() on a button with a menu would pop the menu up */
				emit t->clicked(t->isChecked());
			} else if (!menu()) {
				t->click();
			}
			/* else: the shared menu was shown on press, nothing to do */
		} else if (fallbackClick) {
			fallbackClick();
		}
		QTimer::singleShot(0, this, [this]() { Sync(); });
	}

	QPointer<QAbstractButton> target;
	Width widthMode;
	bool split = false;
};

/* ------------------------------------------------------------------------ */

/* Base for a dock content widget with one horizontal row of controls. */
class HorizontalPanel : public QWidget {
public:
	HorizontalPanel()
	{
		setProperty(kOwnedProperty, true);

		auto outer = new QVBoxLayout(this);
		outer->setContentsMargins(4, 4, 4, 4);
		outer->setSpacing(0);

		row = new QHBoxLayout();
		row->setSpacing(4);
		outer->addLayout(row);
		outer->addStretch(1);

		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

		/* Polling keeps the mirror simple and robust: OBS changes button
		 * texts and classes without emitting any signal we could use. */
		timer = new QTimer(this);
		timer->setInterval(150);
		connect(timer, &QTimer::timeout, this, [this]() { Tick(); });
		timer->start();

		obs_frontend_add_event_callback(OnEvent, this);
	}

	~HorizontalPanel() override { obs_frontend_remove_event_callback(OnEvent, this); }

	/* Registers the dock; call from obs_module_load */
	static bool Register(HorizontalPanel *panel, const char *id, const char *title)
	{
		if (!obs_frontend_add_dock_by_id(id, title, panel)) {
			blog(LOG_WARNING, "[%s] could not register dock '%s'", PLUGIN_NAME, id);
			delete panel;
			return false;
		}
		return true;
	}

protected:
	/* Find originals (only when missing) and copy their state */
	virtual void Tick() = 0;
	virtual void FrontendEvent(enum obs_frontend_event) {}

	/* Throttle the (relatively costly) widget-tree search while an
	 * original is missing */
	bool ShouldSearch()
	{
		if (searchCooldown > 0) {
			searchCooldown--;
			return false;
		}
		searchCooldown = 10;
		return true;
	}
	void SearchNow() { searchCooldown = 0; }

	bool Exiting() const { return exiting; }

	QHBoxLayout *row = nullptr;

private:
	static void OnEvent(enum obs_frontend_event event, void *data)
	{
		auto self = static_cast<HorizontalPanel *>(data);
		if (event == OBS_FRONTEND_EVENT_EXIT) {
			self->exiting = true;
			self->timer->stop();
		}
		self->SearchNow();
		self->FrontendEvent(event);
		if (!self->exiting)
			QTimer::singleShot(0, self, [self]() { self->Tick(); });
	}

	QTimer *timer = nullptr;
	int searchCooldown = 0;
	bool exiting = false;
};

} // namespace hpanels
