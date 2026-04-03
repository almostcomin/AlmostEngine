#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderStages/LinearizeDepthRenderStage.h"
#include "Gfx/RenderStages/SSAORenderStage.h"
#include "Gfx/RenderStages/WireframeRenderStage.h"
#include "Gfx/RenderStages/WBOITAccumRenderStage.h"
#include "Gfx/RenderStages/WBOITResolveRenderStage.h"
#include "Gfx/RenderStages/BloomRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"

class OutdoorsApp : public alm::App
{
public:

	OutdoorsApp() : alm::App{ "OutdoorsApp", alm::App::RenderStageSetMode::User } {}
	~OutdoorsApp() override = default;

	bool Initialize(const AppArgs& args) override
	{
		return true;
	}

	bool Update(float deltaTime) override
	{
		return true;
	}

	void Shutdown() override
	{
	}

	void OnSDLEvent(const SDL_Event& event) override
	{
	}

	std::shared_ptr<alm::gfx::ImGuiRenderStage> UserInitRenderStages() override
	{
		auto shadowmapRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::ShadowmapRenderStage>();
		auto depthPrepassRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::DepthPrepassRenderStage>();
		auto linearizeDepthRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::LinearizeDepthRenderStage>();
		auto SSAORS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::SSAORenderStage>();
		auto GBuffersRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::GBuffersRenderStage>();
		auto deferredLightingRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::DeferredLightingRenderStage>();
		auto simpleSkyRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::SkyRenderStage>();
		auto WBOITAccumRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITAccumRenderStage>();
		auto WBOITResolveRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITResolveRenderStage>();
		auto bloomRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::BloomRenderStage>();
		auto toneMappingRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::ToneMappingRenderStage>();
		auto debugRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::DebugRenderStage>();
		auto wireframeRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::WireframeRenderStage>();
		auto compositeRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::CompositeRenderStage>();
		auto ImGuiRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::ImGuiRenderStage>();

		alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();
		renderGraph->SetRenderStages({
			shadowmapRS,
			depthPrepassRS,
			linearizeDepthRS,
			GBuffersRS,
			SSAORS,
			deferredLightingRS,
			simpleSkyRS,
			WBOITAccumRS,
			WBOITResolveRS,
			bloomRS,
			toneMappingRS,
			debugRS,
			ImGuiRS,
			compositeRS,
			wireframeRS });

		// Define default render mode
		renderGraph->SetRenderMode("Default", {
			shadowmapRS.get(),
			depthPrepassRS.get(),
			linearizeDepthRS.get(),
			GBuffersRS.get(),
			SSAORS.get(),
			deferredLightingRS.get(),
			simpleSkyRS.get(),
			WBOITAccumRS.get(),
			WBOITResolveRS.get(),
			bloomRS.get(),
			toneMappingRS.get(),
			debugRS.get(),
			ImGuiRS.get(),
			compositeRS.get() });

		return ImGuiRS;
	}

private:

};

std::unique_ptr<alm::App> CreateApp()
{
	return std::unique_ptr<alm::App>{ new OutdoorsApp };
}
