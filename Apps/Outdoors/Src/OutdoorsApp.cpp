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
#include "Gfx/TextureLoader.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/TextureCache.h"
#include "Gfx/LoadedTexture.h"
#include "OutdoorsUI.h"

class OutdoorsApp : public alm::fw::App
{
public:

	OutdoorsApp() : alm::fw::App{ "OutdoorsApp", alm::fw::App::RenderStageSetMode::User } {}
	~OutdoorsApp() override = default;

	bool Initialize() override
	{
		m_MainCamera->SetPosition(float3{ 0.f, 0.f, 100.f });

		m_CameraController.SetWindow(m_Window);
		m_CameraController.SetCamera(m_MainCamera);
		m_CameraController.SetSpeed(200.f);

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

		// Set clouds texture
		if (std::filesystem::exists("Generated/CloudShape.dds"))
		{
			alm::gfx::TextureCache* txtrCache = m_DeviceManager->GetTextureCache();
			auto loadResult = txtrCache->Load("Generated/CloudShape.dds", alm::gfx::TextureCache::Flags::None);
			if (!loadResult)
			{
				LOG_ERROR("Failed loading CloudsShape.dds\n{}", loadResult.error());
			}
			else
			{
				loadResult->second.Wait();
				alm::rhi::TextureOwner cloudsTexture = std::move(loadResult->first->texture);
				m_SkyRS->SetCloudsShapeTexture(std::move(cloudsTexture));
			}
		}
		else
		{
			auto createResult = alm::gfx::SkyRenderStage::CreateCloudsShapeTexture(m_DeviceManager.get());
			assert(createResult);
			createResult->second.Wait();
			alm::rhi::TextureOwner& cloudsTexture = createResult->first;
			alm::gfx::SaveDDSTexture(cloudsTexture.get_weak(), alm::rhi::ResourceState::SHADER_RESOURCE, alm::rhi::ResourceState::SHADER_RESOURCE,
				m_DeviceManager->GetDevice(), "Generated/CloudShape.dds");

			m_SkyRS->SetCloudsShapeTexture(std::move(cloudsTexture));
		}

		// Init UI params
		m_UI->m_Data.SunParams = m_Scene->GetSunParams();
		m_UI->m_Data.SkyParams = m_SkyRS->GetSkyParams();

		m_UI->AddTextureWindow("CloudShape.dds", m_SkyRS->GetCloudsShapeTexture());

		return true;
	}

	bool Update(float deltaTime) override
	{
		if (m_UI->m_Data.SunParamsUpdated)
		{
			m_Scene->SetSunParams(m_UI->m_Data.SunParams);
			m_UI->m_Data.SunParamsUpdated = false;
		}
		m_SkyRS->SetSkyParams(m_UI->m_Data.SkyParams);

		// Camera movement
		m_CameraController.Update(deltaTime);

		return true;
	}

	void Shutdown() override
	{
		m_SkyRS.reset();
		m_UI.reset();
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
		auto ImGuiRS = alm::gfx::RenderStageFactory::CreateShared<OutdoorsUI>();

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

		m_UI = ImGuiRS;
		m_SkyRS = skyRS;

		toneMappingRS->SetTonemappingEnabled(false);

		return ImGuiRS;
	}

private:

	alm::CameraController m_CameraController;

	std::shared_ptr<alm::gfx::SkyRenderStage> m_SkyRS;
	std::shared_ptr<OutdoorsUI> m_UI;
};

std::unique_ptr<alm::fw::App> CreateApp()
{
	return std::unique_ptr<alm::fw::App>{ new OutdoorsApp };
}
