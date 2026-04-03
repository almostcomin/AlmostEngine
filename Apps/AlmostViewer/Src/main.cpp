#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
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

class AlmostViewerApp : public alm::App
{
public:

	AlmostViewerApp() : alm::App{ "Almost Viewer", alm::App::RenderStageSetMode::Default } {}
	~AlmostViewerApp() override = default;

	bool Initialize(const AppArgs& args) override
	{
		alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();

		m_UIRS = renderGraph->GetRenderStage<StructureUI>();
		m_ShadowmapRS = renderGraph->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
		m_SimpleSkyRS = renderGraph->GetRenderStage<alm::gfx::SkyRenderStage>();
		m_SSAORS = renderGraph->GetRenderStage<alm::gfx::SSAORenderStage>();
		m_BloomRS = renderGraph->GetRenderStage<alm::gfx::BloomRenderStage>();
		m_TonemappingRS = renderGraph->GetRenderStage<alm::gfx::ToneMappingRenderStage>();
		m_CompositeRS = renderGraph->GetRenderStage<alm::gfx::CompositeRenderStage>();
		m_DebugRS = renderGraph->GetRenderStage<alm::gfx::DebugRenderStage>();
		m_LightingRS = renderGraph->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();

		m_UIRS->m_RequestLoadFile = [this](const char* filename) { m_RequestLoadFile = filename; };
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
				m_Scene->SetSceneGraph(std::move(*importResult));

				const alm::math::aabox3f& bounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds(alm::gfx::BoundsType::Mesh);
				const float radius = glm::length(bounds.extents()) / 2.f;
				m_MainCamera->SetZNear(radius * 0.05f);

				m_MainCamera->SetPosition(float3{ -1000.f, 500.f, 1000.f });
				m_MainCamera->Fit(bounds);

				m_UIRS->m_Data.CameraSpeed = radius * 1.f;

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
			if (m_Scene)
			{
				m_Scene->SetSceneGraph(nullptr);
			}
			m_RequestClose = false;
		}

		// Camera movement
		{
			const float3& camFwd = m_MainCamera->GetForward();
			const float3& camRight = m_MainCamera->GetRight();

			float3 newPos = m_MainCamera->GetPosition();
			float2 cameraTurboSpeed = m_CameraSpeed;
			SDL_Keymod mod = SDL_GetModState();
			if (mod & SDL_KMOD_SHIFT)
				cameraTurboSpeed *= 2.f;
			newPos += camFwd * cameraTurboSpeed.y * deltaTime;
			newPos += -camRight * cameraTurboSpeed.x * deltaTime;

			m_MainCamera->SetPosition(newPos);
		}

		// Update UI values
		{
			auto& data = m_UIRS->m_Data;
			if (!data.RenderMode.empty() && data.RenderMode != renderGraph->GetCurrentRenderMode())
			{
				renderGraph->SetCurrentRenderMode(data.RenderMode);
			}

			m_DebugRS->ShowRenderBBoxes(alm::gfx::BoundsType::Mesh, data.ShowMeshBBoxes);
			m_DebugRS->ShowRenderBBoxes(alm::gfx::BoundsType::Light, data.ShowLightBBoxes);

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
		switch (event.type)
		{
		case SDL_EVENT_MOUSE_MOTION:
		{
			if (m_MouseMiddlePressed)
			{
				int windowWidth, windowHeight;
				SDL_GetWindowSize(m_Window, (int*)&windowWidth, (int*)&windowHeight);
				{
					float angleRad = event.motion.yrel * PI / windowHeight;
					glm::quat q = glm::angleAxis(-angleRad, m_MainCamera->GetRight());
					float3 newFwd = q * m_MainCamera->GetForward();
					m_MainCamera->SetForward(newFwd);
				}
				{
					float angleRad = event.motion.xrel * PI / windowWidth;
					glm::quat q = glm::angleAxis(-angleRad, m_MainCamera->GetUp());
					float3 newFwd = q * m_MainCamera->GetForward();
					m_MainCamera->SetForward(newFwd);
				}
			}
		} break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				break;
			case SDL_BUTTON_MIDDLE:
				m_MouseMiddlePressed = event.button.down;
				break;
			case SDL_BUTTON_RIGHT:
				break;
			}
		} break;

		case SDL_EVENT_KEY_DOWN:
		{
			switch (event.key.key)
			{
			case SDLK_W: m_CameraSpeed.y = m_UIRS->m_Data.CameraSpeed; break;
			case SDLK_S: m_CameraSpeed.y = -m_UIRS->m_Data.CameraSpeed; break;
			case SDLK_A: m_CameraSpeed.x = m_UIRS->m_Data.CameraSpeed; break;
			case SDLK_D: m_CameraSpeed.x = -m_UIRS->m_Data.CameraSpeed; break;
			}
		} break;

		case SDL_EVENT_KEY_UP:
		{
			switch (event.key.key)
			{
			case SDLK_W: m_CameraSpeed.y = 0.f; break;
			case SDLK_S: m_CameraSpeed.y = 0.f; break;
			case SDLK_A: m_CameraSpeed.x = 0.f; break;
			case SDLK_D: m_CameraSpeed.x = 0.f; break;
			}
		} break;
		}
	}

	alm::gfx::RenderStageTypeID GetUIRenderStageType() const override { return StructureUI::StaticType(); }

private:

	std::shared_ptr<StructureUI> m_UIRS;
	std::shared_ptr<alm::gfx::ShadowmapRenderStage> m_ShadowmapRS;
	std::shared_ptr<alm::gfx::SkyRenderStage> m_SimpleSkyRS;
	std::shared_ptr<alm::gfx::SSAORenderStage> m_SSAORS;
	std::shared_ptr<alm::gfx::BloomRenderStage> m_BloomRS;
	std::shared_ptr<alm::gfx::ToneMappingRenderStage> m_TonemappingRS;
	std::shared_ptr<alm::gfx::CompositeRenderStage> m_CompositeRS;
	std::shared_ptr<alm::gfx::DebugRenderStage> m_DebugRS;
	std::shared_ptr<alm::gfx::DeferredLightingRenderStage> m_LightingRS;

	std::string m_RequestLoadFile;
	bool m_RequestClose = false;
	bool m_RequestQuit = false;

	bool m_MouseMiddlePressed = false;
	float2 m_CameraSpeed{ 0.f };
};

std::unique_ptr<alm::App> CreateApp()
{
	return std::unique_ptr<alm::App>{ new AlmostViewerApp };
}
