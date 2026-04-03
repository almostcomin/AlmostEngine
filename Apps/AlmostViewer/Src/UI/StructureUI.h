#pragma once

#include "Core/CorePCH.h"
#include "Core/Math/plane.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
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

class StructureUI : public alm::gfx::ImGuiRenderStage
{
	REGISTER_RENDER_STAGE(StructureUI)

public:

	struct Data
	{
		bool ShowUI = true;

		std::string RenderMode;

		float CameraSpeed = 2.f;
		bool ShowMeshBBoxes = false;
		bool ShowLightBBoxes = false;

		bool ShadowmapEnabled = true;
		bool ShowShadowmap = false;
		int ShadowmapDepthBias;
		float ShadowmapSlopeScaledDepthBias;
		int2 ShadowmapSize;
		
		alm::gfx::Scene::AmbientParams AmbientParams;
		bool AmbientParamsUpdated = false;
		alm::gfx::Scene::SunParams SunParams;
		bool SunParamsUpdated = false;

		bool SkyEnabled = true;
		alm::gfx::SkyRenderStage::SkyParams SkyParams;

		alm::gfx::DeferredLightingRenderStage::MaterialChannel MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;

		bool SSAOEnabled = true;
		bool ShowSSAO = false;
		float2 SSAO_NoiseScale;
		float SSAO_Radius;
		float SSAO_Power;
		float SSAO_Bias;

		bool bloomEnabled = true;
		float bloomRadius;
		float bloomStrength;
		int bloomMaxMip;

		bool tonemappingEnabled = true;
		float middleGrayNits = 0.18f;
		float paperWhiteNits = 203.f;
		float minLogLuminance = -10.f;
		float logLuminanceRange = 12.f;
		float adaptationUpSpeed = 2.f;
		float adaptationDownSpeed = 0.5f;
		float sdrExposureBias = 0.5f;
	};

	StructureUI();
	~StructureUI();

	void Init(SDL_Window* window);

	void BuildUI() override;

	void OnSceneChanged() override;

public:

	Data m_Data;

	std::function<void(const char*)> m_RequestLoadFile;
	std::function<void()> m_RequestClose;
	std::function<void()> m_RequestQuit;

private:

	struct RenderStageTextureView
	{
		alm::gfx::RenderStage* renderStage;
		alm::gfx::RenderGraph::AccessMode accessMode;
		std::string id;
		alm::gfx::RGTextureViewTicket ticket;
		bool applyAlpha = false;
		bool redChannel = true;
		bool greenChannel = true;
		bool blueChannel = true;
		bool alphaChannel = false;
		float defaultImageWidth = 360.f;
		bool firstShow = true;
	};

	struct RenderStageBufferView
	{
		alm::gfx::RenderStage* renderStage;
		alm::gfx::RenderGraph::AccessMode accessMode;
		std::string id;
		alm::gfx::RGBufferViewTicket ticket;
		std::unique_ptr<MemoryEditor> memEditor;
	};

private:

	void OnAttached() override;

	void BuildMainMenu();
	void BuildMenuFile();

	void BuildSettingsWindow();
	void BuildSceneWindow(bool* p_open);
	void BuildResourcesWindow(bool* p_open);
	void BuildRenderStagesWindow();
	void BuildRSViews();
	void BuildLumnincaHistogram();

	bool BuildRSTexView(RenderStageTextureView* rsTexView);
	bool BuildRSBufferView(RenderStageBufferView* rsBufferView);

	void BuildMeshInstanceLeaf(const alm::gfx::MeshInstance* leaf);
	void BuildDirLightLeaf(const alm::gfx::SceneDirectionalLight* light);
	void BuildPointLightLeaf(const alm::gfx::ScenePointLight* light);
	void BuildSpotLightLeaf(const alm::gfx::SceneSpotLight* light);

	void AddRenderStageTextureView(alm::gfx::RenderStage* renderStage, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& id);
	void AddRenderStageBufferView(alm::gfx::RenderStage* renderStage, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& id);

	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

private:

	SDL_Window* m_Window;
	alm::gfx::DeviceManager* m_DeviceManager;
	alm::gfx::RenderView* m_RenderView;

	bool m_ShowSettings;
	int2 m_ShadowmapSize;
	std::shared_ptr<alm::gfx::ShadowmapRenderStage> m_ShadowmapRS;

	bool m_ShowSceneWindow;
	const alm::gfx::SceneGraphNode* m_SelectedNode;

	bool m_ShowResourcesWindow;

	bool m_ShowRenderStages;
	std::string m_RenderStageIOHoveredId;

	std::vector<RenderStageTextureView> m_RSTextureViews;
	std::vector<RenderStageBufferView> m_RSBufferViews;
	alm::gfx::RGTextureViewTicket m_RSTexViewFocus;
	alm::gfx::RGBufferViewTicket m_RSBufferViewFocus;

	bool m_ShowLuminanceHistogram;
	alm::gfx::RGBufferViewTicket m_LumHistogramBufferTicket;
	int m_LumHistogramMode = 0;
	std::shared_ptr<alm::gfx::ToneMappingRenderStage> m_TonemappingRS;
};