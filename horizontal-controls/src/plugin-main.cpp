/*
 * Horizontal Controls - the OBS "Controls" dock with all buttons in one row.
 */
#include "hpanels.hpp"
#include "plugin-version.h"

#include <QAction>

#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

using namespace hpanels;

namespace {

bool OutputExists(obs_output_t *output)
{
	obs_output_release(output);
	return output != nullptr;
}

void SetFallbackState(MirrorButton *b, bool visible, const QString &text, bool active)
{
	b->setVisible(visible);
	if (b->text() != text)
		b->setText(text);
	if (b->toolTip() != text)
		b->setToolTip(text);
	const char *cls = active ? "state-active" : "";
	if (b->property("class").toString() != cls) {
		b->setProperty("class", cls);
		Repolish(b);
	}
}

QAction *SettingsAction()
{
	QMainWindow *main = MainWindow();
	return main ? main->findChild<QAction *>("action_Settings") : nullptr;
}

class HorizontalControls : public HorizontalPanel {
public:
	HorizontalControls()
	{
		setObjectName("horizontalControlsPanel");

		auto add = [this](const char *original, MirrorButton::Width width) {
			auto b = new MirrorButton(this, width);
			b->setObjectName(QString("hc_") + original);
			row->addWidget(b);
			buttons.push_back({original, b});
			return b;
		};

		/* Same order as the stock dock, top to bottom -> left to right */
		MirrorButton *stream = add("streamButton", MirrorButton::Width::Text);
		add("broadcastButton", MirrorButton::Width::Text);
		MirrorButton *record = add("recordButton", MirrorButton::Width::Text);
		MirrorButton *pause = add("pauseRecordButton", MirrorButton::Width::Compact);
		MirrorButton *replay = add("replayBufferButton", MirrorButton::Width::Text);
		MirrorButton *saveReplay = add("saveReplayButton", MirrorButton::Width::Compact);
		MirrorButton *vcam = add("virtualCamButton", MirrorButton::Width::Text);
		add("virtualCamConfigButton", MirrorButton::Width::Compact);
		MirrorButton *studio = add("modeSwitch", MirrorButton::Width::Text);
		MirrorButton *settings = add("settingsButton", MirrorButton::Width::Text);

		/* Fallbacks through the public API, used only if OBS ever stops
		 * providing the stock buttons */
		stream->fallbackClick = []() {
			obs_frontend_streaming_active() ? obs_frontend_streaming_stop() : obs_frontend_streaming_start();
		};
		stream->fallbackSync = [](MirrorButton *b) {
			bool on = obs_frontend_streaming_active();
			SetFallbackState(b, true, ObsText(on ? "Basic.Main.StopStreaming" : "Basic.Main.StartStreaming"),
					 on);
		};

		record->fallbackClick = []() {
			obs_frontend_recording_active() ? obs_frontend_recording_stop() : obs_frontend_recording_start();
		};
		record->fallbackSync = [](MirrorButton *b) {
			bool on = obs_frontend_recording_active();
			SetFallbackState(b, true, ObsText(on ? "Basic.Main.StopRecording" : "Basic.Main.StartRecording"),
					 on);
		};

		pause->fallbackClick = []() { obs_frontend_recording_pause(!obs_frontend_recording_paused()); };
		pause->fallbackSync = [](MirrorButton *b) {
			bool paused = obs_frontend_recording_paused();
			SetFallbackState(b, obs_frontend_recording_active(),
					 ObsText(paused ? "Basic.Main.UnpauseRecording" : "Basic.Main.PauseRecording"),
					 paused);
		};

		replay->fallbackClick = []() {
			obs_frontend_replay_buffer_active() ? obs_frontend_replay_buffer_stop()
							    : obs_frontend_replay_buffer_start();
		};
		replay->fallbackSync = [](MirrorButton *b) {
			bool on = obs_frontend_replay_buffer_active();
			SetFallbackState(b, OutputExists(obs_frontend_get_replay_buffer_output()),
					 ObsText(on ? "Basic.Main.StopReplayBuffer" : "Basic.Main.StartReplayBuffer"), on);
		};

		saveReplay->fallbackClick = []() { obs_frontend_replay_buffer_save(); };
		saveReplay->fallbackSync = [](MirrorButton *b) {
			SetFallbackState(b, obs_frontend_replay_buffer_active(), ObsText("Basic.Main.SaveReplay"),
					 false);
		};

		vcam->fallbackClick = []() {
			obs_frontend_virtualcam_active() ? obs_frontend_stop_virtualcam()
							 : obs_frontend_start_virtualcam();
		};
		vcam->fallbackSync = [](MirrorButton *b) {
			bool on = obs_frontend_virtualcam_active();
			SetFallbackState(b, OutputExists(obs_frontend_get_virtualcam_output()),
					 ObsText(on ? "Basic.Main.StopVirtualCam" : "Basic.Main.StartVirtualCam"), on);
		};

		studio->fallbackClick = []() {
			obs_frontend_set_preview_program_mode(!obs_frontend_preview_program_mode_active());
		};
		studio->fallbackSync = [](MirrorButton *b) {
			b->setCheckable(true);
			b->setChecked(obs_frontend_preview_program_mode_active());
			SetFallbackState(b, true, ObsText("Basic.TogglePreviewProgramMode"), false);
		};

		settings->fallbackClick = []() {
			if (QAction *action = SettingsAction())
				action->trigger();
		};
		settings->fallbackSync = [](MirrorButton *b) {
			SetFallbackState(b, SettingsAction() != nullptr, ObsText("Settings"), false);
		};
	}

protected:
	void Tick() override
	{
		if (Exiting())
			return;

		bool missing = false;
		for (const Entry &e : buttons)
			missing |= !e.button->Mirroring();

		if (missing && ShouldSearch()) {
			int found = 0;
			for (const Entry &e : buttons) {
				if (!e.button->Mirroring())
					e.button->SetTarget(FindOriginal<QAbstractButton>(e.original));
				found += e.button->Mirroring();
			}
			if (found != lastFound) {
				blog(LOG_INFO, "[%s] mirroring %d of %d stock control buttons", PLUGIN_NAME, found,
				     (int)buttons.size());
				lastFound = found;
			}
		}

		for (const Entry &e : buttons)
			e.button->Sync();
	}

private:
	struct Entry {
		const char *original;
		MirrorButton *button;
	};
	std::vector<Entry> buttons;
	int lastFound = -1;
};

} // namespace

bool obs_module_load(void)
{
	blog(LOG_INFO, "[%s] version %s loaded", PLUGIN_NAME, PLUGIN_VERSION);
	return HorizontalPanel::Register(new HorizontalControls(), "horizontal_controls_dock",
					 obs_module_text("DockTitle"));
}
