#pragma once

#include "Framework/UI/FrameworkUI.h"
#include "Gfx/NoiseHeightmapSource.h"

namespace alm::gfx
{
	class SceneHeightmap;
}

class OutdoorsUI : public alm::fw::FrameworkUI
{
	REGISTER_RENDER_STAGE(OutdoorsUI)

public:

	void Init(SDL_Window* window, alm::weak<alm::gfx::Scene> scene, alm::weak<alm::gfx::RenderView> renderView, alm::fw::CameraController* cameraController) override;

	void SetHeightmap(alm::weak<alm::gfx::SceneHeightmap> sceneHeightmap) { m_SceneHeightmap = sceneHeightmap; }

private:

	void BuildUI() override;

	void BuildHeightmapMenuItem();

private:

	alm::weak<alm::gfx::SceneHeightmap> m_SceneHeightmap;
	bool m_ShowHeightmapSettings = true;

	bool m_ForceSetMaxDepth;
	int m_ComboDataSource = -1;

	alm::gfx::NoiseHeightmapSource::Params m_NoiseHeightmapParams;
	uint32_t m_NoiseTextureSize = 0;
};