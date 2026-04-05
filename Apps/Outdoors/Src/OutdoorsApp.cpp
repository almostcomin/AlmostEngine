#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
#include "Framework/CameraController.h"
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
#include "Gfx/GltfImporter.h"
#include "Gfx/Camera.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"

class OutdoorsApp : public alm::App
{
public:

	OutdoorsApp() : alm::App{ "OutdoorsApp", alm::App::RenderStageSetMode::User } {}
	~OutdoorsApp() override = default;

	bool Initialize() override
	{
		m_MainCamera->SetPosition(float3{ 0.f, 0.f, 100.f });

		m_CameraController.SetWindow(m_Window);
		m_CameraController.SetCamera(m_MainCamera);

		std::string path = GetStartupArgString("load");
		if (!path.empty())
		{
			auto importResult = alm::gfx::ImportGlTF(path.c_str(), m_DeviceManager.get());
			if (importResult)
			{
				m_Scene->SetSceneGraph(std::move(*importResult));

				const alm::math::aabox3f& bounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds(alm::gfx::BoundsType::Mesh);
				const float radius = glm::length(bounds.extents()) / 2.f;
				m_MainCamera->SetZNear(radius * 0.05f);

				m_MainCamera->SetPosition(float3{ 0.f, 0.f, 100.f });
				m_MainCamera->Fit(bounds);

				m_CameraController.SetSpeed(radius * 1.f);
			}
		}

		return true;
	}

	bool Update(float deltaTime) override
	{
		// Camera movement
		m_CameraController.Update(deltaTime);

		return true;
	}

	void Shutdown() override
	{
	}

	void OnSDLEvent(const SDL_Event& event) override
	{
		m_CameraController.OnSDLEvent(event);
	}

	std::shared_ptr<alm::gfx::ImGuiRenderStage> UserInitRenderStages() override
	{
		auto shadowmapRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::ShadowmapRenderStage>();
		auto depthPrepassRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::DepthPrepassRenderStage>();
		auto linearizeDepthRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::LinearizeDepthRenderStage>();
		auto SSAORS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::SSAORenderStage>();
		auto GBuffersRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::GBuffersRenderStage>();
		auto deferredLightingRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::DeferredLightingRenderStage>();
		auto skyRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::SkyRenderStage>();
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
			skyRS,
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
			skyRS.get(),
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

	alm::CameraController m_CameraController;
};

std::unique_ptr<alm::App> CreateApp()
{
	return std::unique_ptr<alm::App>{ new OutdoorsApp };
}
