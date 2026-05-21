#pragma once

#include "Core/CorePCH.h"
#include "Core/Math/plane.h"
#include "Framework/UI/FrameworkUI.h"	
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/SimpleSkyRenderStage.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/Scene.h"
#include <functional>
#include "Core/Memory.h"
#include "Gfx/RenderStageFactory.h"

struct MemoryEditor;

namespace alm::gfx
{
	class SceneGraphNode;
	class MeshInstance;
	class SceneDirectionalLight;
	class ScenePointLight;
	class SceneSpotLight;
	class DeviceManager;
	class ShadowmapRenderStage;
	class ToneMappingRenderStage;
};

class StructureUI : public alm::fw::FrameworkUI
{
	REGISTER_RENDER_STAGE(StructureUI)

public:

	StructureUI();
	~StructureUI();

	void BuildUI() override;

private:

	void OnAttached() override;

private:
};