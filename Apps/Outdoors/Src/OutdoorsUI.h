#pragma once

#include "Framework/UI/FrameworkUI.h"

namespace alm::gfx
{
	class SceneHeightmap;
}

class HeightmapSettingsWindow;

class OutdoorsUI : public alm::fw::FrameworkUI
{
	REGISTER_RENDER_STAGE(OutdoorsUI)

public:

	OutdoorsUI();

	void Init(SDL_Window* window, alm::weak<alm::gfx::Scene> scene, alm::weak<alm::gfx::RenderView> renderView, alm::fw::CameraController* cameraController) override;

	void SetHeightmap(alm::weak<alm::gfx::SceneHeightmap> sceneHeightmap);

private:

	void BuildHeightmapMenuItem();

private:

	HeightmapSettingsWindow* m_HeightmapWindow = nullptr;

	// Kept for the "Heightmap" menu item enable state and the source data view
	alm::weak<alm::gfx::SceneHeightmap> m_SceneHeightmap;
};
