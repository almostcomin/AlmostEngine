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
#include "Gfx/RenderStages/CloudsRenderStage.h"
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
#include "Gfx/CommonResources.h"
#include "Gfx/LoadedTexture.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/ImageHeightmapSource.h"
#include "Gfx/NoiseHeightmapSource.h"
#include "Gfx/Heightmap.h"
#include "Gfx/TerrainMaterial.h"
#include "OutdoorsUI.h"
#include <SDL3/SDL_keyboard.h>

class OutdoorsApp : public alm::fw::App
{
public:

	static constexpr float kEarthRadius = 63600.f;// 1000.f;//6360000.f;
	static constexpr float3 kEarthPos = { 0.f, -kEarthRadius, 0.f };

	OutdoorsApp() : alm::fw::App{ "OutdoorsApp", alm::fw::App::RenderStageSetMode::User } {}
	~OutdoorsApp() override = default;

	bool Initialize() override
	{
		alm::gfx::TextureCache* textureCache = m_DeviceManager->GetTextureCache();

		m_MainCamera->SetPosition(float3{ 0.f, 0.f, 1.f });

		m_CameraController.SetWindow(m_Window);
		m_CameraController.SetCamera(m_MainCamera);
		m_CameraController.SetSpeed(1.f);
		m_MainCamera->SetZNear(0.01f);

		// Init earth sphere
#if 0
		{
			alm::gfx::CommonResources* commonResources = m_DeviceManager->GetCommonResources();
			auto earthMesh = commonResources->CreateUVSphere(kEarthRadius, 128, 128, m_DeviceManager->GetDataUploader(), "EarthMesh");

			auto meshInstance = alm::make_unique_with_weak<alm::gfx::MeshInstance>(earthMesh);			
			meshInstance->SetInstanceFlags(meshInstance->GetInstanceFlags() & ~alm::gfx::MeshInstance::Flags::CastShadows);

			auto graphNode = alm::make_unique_with_weak<alm::gfx::SceneGraphNode>();
			graphNode->SetName("EarthSphere");
			graphNode->SetLeaf(std::move(meshInstance));
			graphNode->SetLocalTransform(alm::gfx::Transform().SetTranslation(kEarthPos));

			auto sceneGraph = m_Scene->GetSceneGraph();
			sceneGraph->GetRoot()->AddChild(std::move(graphNode));
			m_Scene->RefreshSceneGraph();
		}
#endif
		// Init heightmap
		std::shared_ptr<alm::gfx::Heightmap> heightmap;
		alm::weak<alm::gfx::SceneHeightmap> sceneHeightmap;
		{
			// Data source
			std::shared_ptr<alm::gfx::IHeightmapSource> dataSource;
			{
#if 0
				alm::gfx::NoiseHeightmapSource::Params params{
					.Frequency = 8.f,
					.Octaves = 6 };

				auto noiseSource = std::make_shared<alm::gfx::NoiseHeightmapSource>(params, "NoiseHeightmap");
				dataSource = noiseSource;					
#else
				auto imageSource = std::make_shared<alm::gfx::ImageHeightmapSource>(
					alm::gfx::ImageHeightmapSource::EdgeMode::Clamp);
				bool sourceOk = imageSource->Load("SpainHeightmap.png");
				assert(sourceOk);

				dataSource = imageSource;
#endif
			}

			// Material
			auto mat = std::make_shared<alm::gfx::TerrainMaterial>("Heightmap");
			{
				alm::gfx::TextureCache::LoadResult loadResult;

				// Ground Color
				loadResult = textureCache->Load(
					"Textures/Ground037/Ground037_1K-PNG_Color.png",
					alm::gfx::TextureCache::Flags::ForceSRGB | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Ground.BaseColorTexture = loadResult->first;
				}
				// Ground Normal
				loadResult = textureCache->Load(
					"Textures/Ground037/Ground037_1K-PNG_NormalGL.png",
					alm::gfx::TextureCache::Flags::IsNormalMap | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Ground.NormalTexture = loadResult->first;
				}
				// Ground ORM
				loadResult = textureCache->Load(
					"Textures/Ground037/Ground037_1K-PNG_ORM.png",
					alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Ground.MetalRoughTexture = loadResult->first;
				}
				mat->Ground.UVScale = 60.f;

				// Peak Color
				loadResult = textureCache->Load(
					"Textures/Snow010A/Snow010A_1K-PNG_Color.png",
					alm::gfx::TextureCache::Flags::ForceSRGB | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Peak.BaseColorTexture = loadResult->first;
				}
				// Peak Normal
				loadResult = textureCache->Load(
					"Textures/Snow010A/Snow010A_1K-PNG_NormalGL.png",
					alm::gfx::TextureCache::Flags::IsNormalMap | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Peak.NormalTexture = loadResult->first;
				}
				// Peak ORM
				loadResult = textureCache->Load(
					"Textures/Snow010A/Snow010A_1K-PNG_ORM.png",
					alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Peak.MetalRoughTexture = loadResult->first;
				}
				mat->Peak.UVScale = 60.f;

				// Slope Color
				loadResult = textureCache->Load(
					"Textures/Rock050/Rock050_1K-PNG_Color.png",
					alm::gfx::TextureCache::Flags::ForceSRGB | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Slope.BaseColorTexture = loadResult->first;
				}
				// Slope Normal
				loadResult = textureCache->Load(
					"Textures/Rock050/Rock050_1K-PNG_NormalGL.png",
					alm::gfx::TextureCache::Flags::IsNormalMap | alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Slope.NormalTexture = loadResult->first;
				}
				// Slope ORM
				loadResult = textureCache->Load(
					"Textures/Rock050/Rock050_1K-PNG_ORM.png",
					alm::gfx::TextureCache::Flags::GenerateMips);
				if (loadResult)
				{
					mat->Slope.MetalRoughTexture = loadResult->first;
				}
				mat->Slope.UVScale = 60.f;

				//mat->SlopeAngleStartDeg = 15.f;
				//mat->SlopeAngleEndDeg = 20.f;
			}

			// Heightmap
			heightmap = std::make_shared<alm::gfx::Heightmap>(m_DeviceManager.get());
			heightmap->Init(dataSource, mat,
				dataSource->InfiniteDataResolution() ? uint2{ 1024, 1042 } : dataSource->GetDataResolution());

			// Leaf
			auto sceneHeightmapUnique = alm::make_unique_with_weak<alm::gfx::SceneHeightmap>();
			sceneHeightmapUnique->SetHeightmap(heightmap);
			sceneHeightmap = sceneHeightmapUnique.get_weak();

			// Node
			auto graphNode = alm::make_unique_with_weak<alm::gfx::SceneGraphNode>();
			graphNode->SetName("Heightmap");
			graphNode->SetLeaf(std::move(sceneHeightmapUnique));
			graphNode->SetLocalTransform(alm::gfx::Transform().SetScale({ 12.f, 1.2f, 12.f }));
			//graphNode->SetLocalTransform(alm::gfx::Transform().SetTranslation(kEarthPos));

			// Attach to scene
			auto sceneGraph = m_Scene->GetSceneGraph();
			sceneGraph->GetRoot()->AddChild(std::move(graphNode));
		}

		// Load file
		{
			std::string path = GetStartupArgString("load");
			if (!path.empty())
			{
				auto importResult = alm::gfx::ImportGlTF(path.c_str(), m_DeviceManager.get());
				if (importResult)
				{
					m_Scene->GetSceneGraph()->GetRoot()->AddChild(std::move(*importResult));
				}
			}
		}

		// Set Sky params
		{
			m_SkyRS->SetEarthCenter(float3{ 0.f, -kEarthRadius, 0.f });
			m_SkyRS->SetEarthRadius(kEarthRadius, 1.f);
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
				m_CloudsRS->SetCloudsShapeTexture(std::move(cloudsTexture));
			}
		}
		else
		{
			auto createResult = alm::gfx::CloudsRenderStage::CreateCloudsShapeTexture(m_DeviceManager.get());
			assert(createResult);
			createResult->second.Wait();
			alm::rhi::TextureOwner& cloudsTexture = createResult->first;
			alm::gfx::SaveDDSTexture(cloudsTexture.get_weak(), alm::rhi::ResourceState::SHADER_RESOURCE, alm::rhi::ResourceState::SHADER_RESOURCE,
				m_DeviceManager->GetDevice(), "Generated/CloudShape.dds");

			m_CloudsRS->SetCloudsShapeTexture(std::move(cloudsTexture));
		}

		// Init UI
		{
			m_UI->Init(m_Window, m_Scene.get_weak(), m_MainRenderView.get_weak(), &m_CameraController);
			m_UI->SetHeightmap(sceneHeightmap);
			RefreshUIData();
		}

		return true;
	}

	bool Update(float deltaTime) override
	{
		// Camera movement
		m_CameraController.Update(deltaTime);

		// Update UI
		m_UI->AddBottomBarText(std::format("X: {:1.3f}, Y: {:1.3f}, Z: {:1.3f}",
			m_MainCamera->GetPosition().x, m_MainCamera->GetPosition().y, m_MainCamera->GetPosition().z));
		
		float altutide = glm::distance(m_MainCamera->GetPosition(), kEarthPos) - kEarthRadius;
		m_UI->AddBottomBarText(std::format("Altitude: {:1.3f}", altutide));

		return true;
	}

	void Shutdown() override
	{
		m_SkyRS.reset();
		m_CloudsRS.reset();
		m_UI.reset();
	}

	void OnSDLEvent(const SDL_Event& event) override
	{
		switch (event.type)
		{
		case SDL_EVENT_KEY_DOWN:
		{
			switch (event.key.key)
			{
			case SDLK_W:
				if (event.key.mod & SDL_KMOD_CTRL)
				{
					if(!m_Wireframe)
						m_MainRenderView->GetRenderGraph()->SetActiveRenderMode("wireframe");
					else
						m_MainRenderView->GetRenderGraph()->SetActiveRenderMode("default");
					m_Wireframe = !m_Wireframe;
				}
				break;
			}
		} break;
		}

		const bool* keyboardState = SDL_GetKeyboardState(NULL);
		const bool ctrlPressed = keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_RCTRL];
		if (!ctrlPressed)
		{
			m_CameraController.OnSDLEvent(event);
		}
		else
		{
			m_CameraController.Stop();
		}
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
		auto cloudsRS = alm::gfx::RenderStageFactory::CreateShared<alm::gfx::CloudsRenderStage>();
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
			cloudsRS,
			WBOITAccumRS,
			WBOITResolveRS,
			bloomRS,
			toneMappingRS,
			debugRS,
			ImGuiRS,
			compositeRS,
			wireframeRS });

		// Define default render mode
		renderGraph->SetRenderMode("default", {
			shadowmapRS.get(),
			depthPrepassRS.get(),
			linearizeDepthRS.get(),
			GBuffersRS.get(),
			SSAORS.get(),
			deferredLightingRS.get(),
			skyRS.get(),
			cloudsRS.get(),
			WBOITAccumRS.get(),
			WBOITResolveRS.get(),
			bloomRS.get(),
			toneMappingRS.get(),
			debugRS.get(),
			ImGuiRS.get(),
			compositeRS.get() });

		// Define wireframe render mode
		renderGraph->SetRenderMode("wireframe", {
			depthPrepassRS.get(),
			wireframeRS.get(),
			debugRS.get(),
			ImGuiRS.get(),
			compositeRS.get() });

		m_UI = ImGuiRS;
		m_SkyRS = skyRS;
		m_CloudsRS = cloudsRS;

		toneMappingRS->SetTonemappingEnabled(false);

		return ImGuiRS;
	}

private:

	alm::fw::CameraController m_CameraController;

	std::shared_ptr<alm::gfx::SkyRenderStage> m_SkyRS;
	std::shared_ptr<alm::gfx::CloudsRenderStage> m_CloudsRS;
	std::shared_ptr<OutdoorsUI> m_UI;

	bool m_Wireframe = false;
};

std::unique_ptr<alm::fw::App> CreateApp()
{
	return std::unique_ptr<alm::fw::App>{ new OutdoorsApp };
}
