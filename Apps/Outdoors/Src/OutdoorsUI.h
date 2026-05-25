#pragma once

#include "Framework/UI/FrameworkUI.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"

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

	void BuildHeightmapMeniItem();

private:

	alm::weak<alm::gfx::SceneHeightmap> m_SceneHeightmap;
	bool m_ShowHeightmapSettings = true;

	bool m_ForceSetMaxDepth;
};