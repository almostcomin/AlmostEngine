#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
#include "Framework/CameraController.h"
#include "UI/StructureUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/SSAORenderStage.h"
#include "Gfx/RenderStages/BloomRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/GltfImporter.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/Camera.h"
#include <SDL3/SDL.h>
#include <imgui/imgui_impl_sdl3.h>

class AlmostViewerApp : public alm::fw::App
{
public:

	AlmostViewerApp() : alm::fw::App{ "Almost Viewer", alm::fw::App::RenderStageSetMode::Default } {}
	~AlmostViewerApp() override = default;

	bool Initialize() override
	{
		alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();

		m_UIRS = renderGraph->GetRenderStage<StructureUI>();
		m_ShadowmapRS = renderGraph->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
		m_SimpleSkyRS = renderGraph->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
		m_SSAORS = renderGraph->GetRenderStage<alm::gfx::SSAORenderStage>();
		m_BloomRS = renderGraph->GetRenderStage<alm::gfx::BloomRenderStage>();
		m_TonemappingRS = renderGraph->GetRenderStage<alm::gfx::ToneMappingRenderStage>();
		m_CompositeRS = renderGraph->GetRenderStage<alm::gfx::CompositeRenderStage>();
		m_DebugRS = renderGraph->GetRenderStage<alm::gfx::DebugRenderStage>();
		m_LightingRS = renderGraph->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();

		m_UIRS->m_RequestLoadFile = [this](const char* filename) { m_RequestLoadFile = filename; m_bMergeFile = false; };
		m_UIRS->m_RequestMergeFile = [this](const char* filename) { m_RequestLoadFile = filename; m_bMergeFile = true; };
		m_UIRS->m_RequestClose = [this] { m_RequestClose = true; };
		m_UIRS->m_RequestQuit = [this] { m_RequestQuit = true; };
		m_UIRS->Init(m_Window, m_Scene.get_weak(), m_MainRenderView.get_weak(), &m_CameraController);

		m_CameraController.SetWindow(m_Window);
		m_CameraController.SetCamera(m_MainCamera);

		return true;
	}

	bool Update(float deltaTime) override
	{
		if (!m_RequestLoadFile.empty())
		{
			auto importResult = alm::gfx::ImportGlTF(m_RequestLoadFile.c_str(), m_DeviceManager.get());
			if (importResult)
			{
				auto rootNode = m_Scene->GetSceneGraph()->GetRoot();
				if (!m_bMergeFile)
				{
					while (rootNode->GetChildrenCount() > 0)
					{
						rootNode->RemoveChild(rootNode->GetChild(0));
					}
				}
				m_Scene->GetSceneGraph()->GetRoot()->AddChild(std::move(*importResult));

				m_Scene->Update(); // For bounds

				const alm::aabox3f& bounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds(alm::gfx::SceneContentType::Meshes);
				if (bounds.valid())
				{
					const float radius = glm::length(bounds.diagonal()) / 2.f;
					m_MainCamera->SetZNear(radius * 0.05f);

					m_MainCamera->SetPosition(float3{ -1000.f, 500.f, 1000.f });
					m_MainCamera->Fit(bounds);

					m_CameraController.SetSpeed(radius * 1.f);
				}

				m_Scene->GetSceneGraph()->LogGraph();
			}
			else
			{
				LOG_ERROR("Error importing file '{}': {}", m_RequestLoadFile, importResult.error());
			}
			m_RequestLoadFile.clear();
		}

		if (m_RequestClose)
		{
			auto rootNode = m_Scene->GetSceneGraph()->GetRoot();
			while (rootNode->GetChildrenCount() > 0)
			{
				rootNode->RemoveChild(rootNode->GetChild(0));
			}

			m_RequestClose = false;
		}

		// Camera movement
		m_CameraController.Update(deltaTime);

		if (m_RequestQuit)
			return false;

		return true;
	}

	void Shutdown() override
	{
		m_CompositeRS.reset();
		m_TonemappingRS.reset();
		m_BloomRS.reset();
		m_SSAORS.reset();
		m_SimpleSkyRS.reset();
		m_ShadowmapRS.reset();
		m_UIRS.reset();
	}

	void OnSDLEvent(const SDL_Event& event) override
	{
		m_CameraController.OnSDLEvent(event);
	}

	alm::gfx::RenderStageTypeID GetUIRenderStageType() const override { return StructureUI::StaticType(); }

private:

	std::shared_ptr<StructureUI> m_UIRS;
	std::shared_ptr<alm::gfx::ShadowmapRenderStage> m_ShadowmapRS;
	std::shared_ptr<alm::gfx::SimpleSkyRenderStage> m_SimpleSkyRS;
	std::shared_ptr<alm::gfx::SSAORenderStage> m_SSAORS;
	std::shared_ptr<alm::gfx::BloomRenderStage> m_BloomRS;
	std::shared_ptr<alm::gfx::ToneMappingRenderStage> m_TonemappingRS;
	std::shared_ptr<alm::gfx::CompositeRenderStage> m_CompositeRS;
	std::shared_ptr<alm::gfx::DebugRenderStage> m_DebugRS;
	std::shared_ptr<alm::gfx::DeferredLightingRenderStage> m_LightingRS;

	alm::fw::CameraController m_CameraController;

	std::string m_RequestLoadFile;
	bool m_bMergeFile = false;
	bool m_RequestClose = false;
	bool m_RequestQuit = false;	
};

std::unique_ptr<alm::fw::App> CreateApp()
{
	return std::unique_ptr<alm::fw::App>{ new AlmostViewerApp };
}
