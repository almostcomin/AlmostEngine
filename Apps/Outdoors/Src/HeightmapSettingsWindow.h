#pragma once

#include "Framework/UI/UIWindow.h"
#include "Gfx/NoiseHeightmapSource.h"

namespace alm::gfx
{
	class SceneHeightmap;
}

// "Heightmap Settings" window of the Outdoors app, registered into the FrameworkUI
// window registry by OutdoorsUI. Not listed in the View menu: it opens from the
// app's own "Heightmap" main menu.
class HeightmapSettingsWindow : public alm::fw::UIWindow
{
public:

	explicit HeightmapSettingsWindow(alm::fw::FrameworkUI& ui)
		: UIWindow(ui)
	{
		m_Open = true; // shown by default on app start (previous behavior)
	}

	const char* GetName() const override { return "Heightmap Settings"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

	void SetHeightmap(alm::weak<alm::gfx::SceneHeightmap> sceneHeightmap) { m_SceneHeightmap = sceneHeightmap; }
	bool HasHeightmap() const { return m_SceneHeightmap != nullptr; }

private:

	alm::weak<alm::gfx::SceneHeightmap> m_SceneHeightmap;

	bool m_ForceSetMaxDepth = false;
	int m_ComboDataSource = -1;
	bool m_FreezeTesselation = false;

	alm::gfx::NoiseHeightmapSource::Params m_NoiseHeightmapParams;
	uint32_t m_NoiseTextureSize = 0;
	int m_DebugChannel = 0;
};
