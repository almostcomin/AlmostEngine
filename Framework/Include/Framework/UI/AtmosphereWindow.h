#pragma once

// "Atmosphere / Lighting" window (View > Atmosphere / Lighting): lighting mode,
// sky background source and the sky/clouds sections. It also shows the shared
// Shadowmap, Directional light and Ambient widgets (see CommonWidgets.h).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Framework/UI/CloudsPresets.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"

namespace alm::fw
{

class AtmosphereWindow : public UIWindow
{
public:

	explicit AtmosphereWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Atmosphere / Lighting"; }
	void Build() override;

private:

	void BuildSimpleSkySettings();
	void BuildSimSkySettings();
	void BuildCloudsSettings();
	void BuildCubemapSettings(float availWidth);

	CloudsPreset m_CloudsPreset = CloudsPreset::FairWeather;
	alm::gfx::CloudsRenderStage::DebugChannel m_CloudsDebugChannel = alm::gfx::CloudsRenderStage::DebugChannel::Disabled;
};

} // namespace alm::fw
