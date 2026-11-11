#pragma once

// "Settings" window (View > Settings): render modes, camera, debug views and the
// shared Shadowmap / Directional light / Ambient sections (see CommonWidgets.h).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"

namespace alm::fw
{

class SettingsWindow : public UIWindow
{
public:

	explicit SettingsWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Settings"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_S; }
	void Build() override;

private:

	void BuildRenderModesSettings();
	void BuildCameraSettings(float availWidth);
	void BuildDebugViewSettings(float availWidth);
	void BuildMaterialChannelsSettings(float availWidth);
	void BuildSSAOSettings(float availWidth);
	void BuildBloomSettings(float availWidth);
	void BuildExposureSettings(float availWidth);
	void BuildTonemappingSettings(float availWidth);
};

} // namespace alm::fw
