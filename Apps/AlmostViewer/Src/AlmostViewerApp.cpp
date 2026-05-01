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

		// Update UI data with initial render stages values
		m_UIRS->m_Data.ShadowmapSize = m_ShadowmapRS->GetSize();
		m_UIRS->m_Data.ShadowmapDepthBias = m_ShadowmapRS->GetDepthBias();
		m_UIRS->m_Data.ShadowmapSlopeScaledDepthBias = m_ShadowmapRS->GetSlopeScaledDepthBias();

		m_UIRS->m_Data.AmbientParams = m_Scene->GetAmbientParams();
		m_UIRS->m_Data.SunParams = m_Scene->GetSunParams();

		m_UIRS->m_Data.SkyParams = m_SimpleSkyRS->GetSkyParams();

		m_UIRS->m_Data.SSAO_Radius = m_SSAORS->GetRadius();
		m_UIRS->m_Data.SSAO_Power = m_SSAORS->GetPower();
		m_UIRS->m_Data.SSAO_Bias = m_SSAORS->GetBias();

		m_UIRS->m_Data.bloomRadius = m_BloomRS->GetFilterRadius();
		m_UIRS->m_Data.bloomStrength = m_BloomRS->GetStrength();
		m_UIRS->m_Data.bloomMaxMip = m_BloomRS->GetMaxMipChainLenght();

		m_UIRS->m_Data.middleGrayNits = m_TonemappingRS->GetSceneMiddleGray() * m_CompositeRS->GetPaperWhiteNits();
		m_UIRS->m_Data.paperWhiteNits = m_CompositeRS->GetPaperWhiteNits();
		m_UIRS->m_Data.sdrExposureBias = m_TonemappingRS->GetSDRExposureBias();
		m_UIRS->m_Data.minLogLuminance = m_TonemappingRS->GetMinLogLuminance();
		m_UIRS->m_Data.logLuminanceRange = m_TonemappingRS->GetLogLuminanceRange();
		m_UIRS->m_Data.adaptationUpSpeed = m_TonemappingRS->GetAdaptationUpSpeed();
		m_UIRS->m_Data.adaptationDownSpeed = m_TonemappingRS->GetAdaptationDownSpeed();

		m_CameraController.SetWindow(m_Window);
		m_CameraController.SetCamera(m_MainCamera);

		// Init scene graph
		auto sceneGraph = alm::make_unique_with_weak<alm::gfx::SceneGraph>();
		m_Scene->SetSceneGraph(std::move(sceneGraph));

		return true;
	}

	bool Update(float deltaTime) override
	{
		alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();

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

				m_Scene->RefreshSceneGraph();

				const alm::math::aabox3f& bounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds(alm::gfx::SceneContentType::Meshes);
				if (bounds.valid())
				{
					const float radius = glm::length(bounds.extents()) / 2.f;
					m_MainCamera->SetZNear(radius * 0.05f);

					m_MainCamera->SetPosition(float3{ -1000.f, 500.f, 1000.f });
					m_MainCamera->Fit(bounds);

					m_CameraController.SetSpeed(radius * 1.f);
					m_UIRS->m_Data.CameraSpeed = radius * 1.f;
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
			m_Scene->RefreshSceneGraph();

			m_RequestClose = false;
		}

		// Camera movement
		m_CameraController.Update(deltaTime);

		// Update UI values
		{
			auto& data = m_UIRS->m_Data;
			if (!data.RenderMode.empty() && data.RenderMode != renderGraph->GetCurrentRenderMode())
			{
				renderGraph->SetActiveRenderMode(data.RenderMode);
			}

			m_CameraController.SetSpeed(data.CameraSpeed);

			m_DebugRS->ShowRenderBBoxes(alm::gfx::SceneContentType::Meshes, data.ShowMeshBBoxes);
			m_DebugRS->ShowRenderBBoxes(alm::gfx::SceneContentType::SpotLights, data.ShowLightBBoxes);

			m_LightingRS->ShowShadowmap(data.ShowShadowmap);
			if (data.ShadowmapDepthBias != m_ShadowmapRS->GetDepthBias())
			{
				m_ShadowmapRS->SetDepthBias(data.ShadowmapDepthBias);
			}
			if (data.ShadowmapSlopeScaledDepthBias != m_ShadowmapRS->GetSlopeScaledDepthBias())
			{
				m_ShadowmapRS->SetSlopeScaledDepthBias(data.ShadowmapSlopeScaledDepthBias);
			}
			if (data.ShadowmapSize != m_ShadowmapRS->GetSize())
			{
				m_ShadowmapRS->SetSize(data.ShadowmapSize);
			}
			if (data.ShadowmapEnabled != m_ShadowmapRS->IsEnabled())
			{
				m_ShadowmapRS->SetEnabled(data.ShadowmapEnabled);
			}

			if (data.AmbientParamsUpdated)
			{
				m_Scene->SetAmbientParams(data.AmbientParams);
				data.AmbientParamsUpdated = false;
			}

			if (data.SunParamsUpdated)
			{
				m_Scene->SetSunParams(data.SunParams);
				data.SunParamsUpdated = false;
			}

			m_SimpleSkyRS->SetEnabled(data.SkyEnabled);
			m_SimpleSkyRS->SetSkyParams(data.SkyParams);

			m_LightingRS->SetMaterialChannel(data.MatChannel);

			if (data.SSAOEnabled != m_SSAORS->IsSSAOEnabled())
			{
				m_SSAORS->SetSSAOEnabled(data.SSAOEnabled);
			}
			m_LightingRS->ShowSSAO(data.ShowSSAO);
			m_SSAORS->SetRadius(data.SSAO_Radius);
			m_SSAORS->SetPower(data.SSAO_Power);
			m_SSAORS->SetBias(data.SSAO_Bias);

			m_BloomRS->SetBloomEnabled(data.bloomEnabled);
			m_BloomRS->SetFilterRadius(data.bloomRadius);
			m_BloomRS->SetStrength(data.bloomStrength);
			m_BloomRS->SetMaxMipChainLenght(data.bloomMaxMip);

			m_TonemappingRS->SetTonemappingEnabled(data.tonemappingEnabled);
			// Scene middlegray is middle_gray_nits / paper_white_nits
			m_TonemappingRS->SetSceneMiddleGray(data.middleGrayNits / data.paperWhiteNits);
			m_TonemappingRS->SetMinLogLuminance(data.minLogLuminance);
			m_TonemappingRS->SetLogLuminanceRange(data.logLuminanceRange);
			m_TonemappingRS->SetSDRExposureBias(data.sdrExposureBias);
			m_TonemappingRS->SetAdaptationUpSpeed(data.adaptationUpSpeed);
			m_TonemappingRS->SetAdaptationDownSpeed(data.adaptationDownSpeed);
		}

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

	alm::CameraController m_CameraController;

	std::string m_RequestLoadFile;
	bool m_bMergeFile = false;
	bool m_RequestClose = false;
	bool m_RequestQuit = false;	
};

std::unique_ptr<alm::fw::App> CreateApp()
{
	return std::unique_ptr<alm::fw::App>{ new AlmostViewerApp };
}
