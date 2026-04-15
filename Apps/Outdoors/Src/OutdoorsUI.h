#pragma once

#include "Framework/FrameworkUI.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderStages/SkyRenderStage.h"

class OutdoorsUI : public alm::fw::FrameworkUI
{
	REGISTER_RENDER_STAGE(OutdoorsUI)

public:

	struct Data
	{
		bool ShowUI = true;

		alm::gfx::Scene::SunParams SunParams;
		alm::gfx::SkyRenderStage::SkyParams SkyParams;
		bool SunParamsUpdated = false;
	};

	Data m_Data;

private:

	void BuildUI() override;
};