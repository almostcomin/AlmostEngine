#pragma once

#include "Core/Math/plane.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/Scene.h"
#include <functional>
#include "Core/Memory.h"

struct MemoryEditor;

namespace st::gfx
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

class StructureUI : public st::gfx::ImGuiRenderStage
{
public:

	struct Data
	{
		bool ShowUI = true;
		float FPS = 0.f;
		float CPUTime = 0.f;
		float GPUTime = 0.f;

		std::string RenderMode;

		float CameraSpeed = 2.f;
		bool ShowMeshBBoxes = false;
		bool ShowLightBBoxes = false;

		bool ShadowmapEnabled = true;
		bool ShowShadowmap = false;
		int ShadowmapDepthBias;
		float ShadowmapSlopeScaledDepthBias;
		int2 ShadowmapSize;
		
		st::gfx::Scene::AmbientParams AmbientParams;
		bool AmbientParamsUpdated = false;
		st::gfx::Scene::SunParams SunParams;
		bool SunParamsUpdated = false;

		st::gfx::DeferredLightingRenderStage::MaterialChannel MatChannel = st::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;

		bool SSAOEnabled = true;
		bool ShowSSAO = false;
		float2 SSAO_NoiseScale;
		float SSAO_Radius;
		float SSAO_Power;
		float SSAO_Bias;

		bool tonemappingEnabled = true;
		float middleGrayNits = 0.18f;
		float paperWhiteNits = 203.f;
		float minLogLuminance = -10.f;
		float logLuminanceRange = 12.f;
		float adaptationUpSpeed = 2.f;
		float adaptationDownSpeed = 0.5f;
		float sdrExposureBias = 0.5f;
	};

	StructureUI(st::weak<st::gfx::RenderView> renderView, SDL_Window* window, st::gfx::ShadowmapRenderStage* shadowmapRS, st::gfx::ToneMappingRenderStage* tonemappingRS, 
		st::gfx::DeviceManager* deviceManager);
	~StructureUI();

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
		st::gfx::RenderStage* renderStage;
		st::gfx::RenderGraph::AccessMode accessMode;
		std::string id;
		st::gfx::RGTextureViewTicket ticket;
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
		st::gfx::RenderStage* renderStage;
		st::gfx::RenderGraph::AccessMode accessMode;
		std::string id;
		st::gfx::RGBufferViewTicket ticket;
		std::unique_ptr<MemoryEditor> memEditor;
	};

private:

	void BuildMainMenu();
	void BuildBottomBar();
	void BuildMenuFile();

	void BuildSettingsWindow();
	void BuildSceneWindow(bool* p_open);
	void BuildResourcesWindow(bool* p_open);
	void BuildRenderStagesWindow();
	void BuildRSViews();
	void BuildLumnincaHistogram();

	bool BuildRSTexView(RenderStageTextureView* rsTexView);
	bool BuildRSBufferView(RenderStageBufferView* rsBufferView);

	void BuildMeshInstanceLeaf(const st::gfx::MeshInstance* leaf);
	void BuildDirLightLeaf(const st::gfx::SceneDirectionalLight* light);
	void BuildPointLightLeaf(const st::gfx::ScenePointLight* light);
	void BuildSpotLightLeaf(const st::gfx::SceneSpotLight* light);

	void AddRenderStageTextureView(st::gfx::RenderStage* renderStage, st::gfx::RenderGraph::AccessMode accessMode, const std::string& id);
	void AddRenderStageBufferView(st::gfx::RenderStage* renderStage, st::gfx::RenderGraph::AccessMode accessMode, const std::string& id);

	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

private:

	SDL_Window* m_Window;
	st::gfx::DeviceManager* m_DeviceManager;
	st::weak<st::gfx::RenderView> m_RenderView;

	bool m_ShowSettings;
	int2 m_ShadowmapSize;
	st::gfx::ShadowmapRenderStage* m_ShadowmapRS;

	bool m_ShowSceneWindow;
	const st::gfx::SceneGraphNode* m_SelectedNode;

	bool m_ShowResourcesWindow;

	bool m_ShowRenderStages;
	std::string m_RenderStageIOHoveredId;

	std::vector<RenderStageTextureView> m_RSTextureViews;
	std::vector<RenderStageBufferView> m_RSBufferViews;
	st::gfx::RGTextureViewTicket m_RSTexViewFocus;
	st::gfx::RGBufferViewTicket m_RSBufferViewFocus;

	bool m_ShowLuminanceHistogram;
	st::gfx::RGBufferViewTicket m_LumHistogramBufferTicket;
	int m_LumHistogramMode = 0;
	st::gfx::ToneMappingRenderStage* m_TonemappingRS;
};