#pragma once

#include "Framework/UI/FrameworkUI.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"

class OutdoorsUI : public alm::fw::FrameworkUI
{
	REGISTER_RENDER_STAGE(OutdoorsUI)

private:

	void BuildUI() override;
};