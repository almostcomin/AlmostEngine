#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStages/SimpleSkyRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/Scene.h"

struct MemoryEditor;

namespace alm::gfx
{
	class SceneGraphNode;
}

namespace alm::fw
{

	class CameraController;

class FrameworkUI : public alm::gfx::ImGuiRenderStage
{
	REGISTER_RENDER_STAGE(FrameworkUI)

public:

	struct
	{
		//std::string RenderMode = "default";
		std::string RequestedRenderMode;

		struct
		{
			bool ShowMeshBBoxes = false;
			bool ShowLightBBoxes = false;
		} Debug;

		int2 ShadowmapSize = { 0, 0 };

		alm::gfx::Scene::SunParams SunParams;
		bool SunParamsUpdated = false;

		alm::gfx::Scene::AmbientParams AmbientParams;
		bool AmbientParamsUpdated = false;

		alm::gfx::SimpleSkyRenderStage::SkyParams SimpleSkyParams;

		alm::gfx::SkyRenderStage::SkyParams SkyParams;
		bool SkyEnabled = true;

		alm::gfx::CloudsRenderStage::CloudsParams CloudsParams;

		alm::gfx::DeferredLightingRenderStage::MaterialChannel MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;

		struct
		{
			bool Enabled = true;
			bool View = false;
			float Radius;
			float Power;
			float Bias;
		} SSAO;

		struct
		{
			bool Enabled = true;
			float Radius;
			float Strength;
			int MaxMip;
		} Bloom;

		struct
		{
			bool Enabled = true;
			float MiddleGrayNits = 0.18f;
			float PaperWhiteNits = 203.f;
			float MinLogLuminance = -10.f;
			float LogLuminanceRange = 12.f;
			float AdaptationUpSpeed = 2.f;
			float AdaptationDownSpeed = 0.5f;
			float SdrExposureBias = 0.5f;
		} Tonemapping;

	} FrameworkData;

public:

	FrameworkUI();
	~FrameworkUI();

	virtual void Init(SDL_Window* window, weak<alm::gfx::Scene> scene, weak<alm::gfx::RenderView> renderView, CameraController* cameraController = nullptr);

	weak<alm::gfx::Scene> GetScene() const { return m_Scene; }

	void ShowBottomBar(bool b) { m_ShowBottomBar = b; }
	void AddBottomBarText(const std::string& text);

	void SetRenderStats(float fps, float cpuTime, float gpuTime);

	void AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture);

	void AddRenderStageTextureWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& textureId);
	void AddRenderStageBufferWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& bufferId);

	void RegisterMainMenuItem(const std::string& name, std::function<void()> item);

	std::function<void(const char*)> m_RequestLoadFile;
	std::function<void(const char*)> m_RequestMergeFile;
	std::function<void()> m_RequestClose;
	std::function<void()> m_RequestQuit;

protected:

	void BuildUI() override;
	
	static bool ShowToggleButton(const char* label, bool* v);
	static void ShowPropertyInt(const char* label, float labelWidth, int value, float valueWidth, int id);
	static void ShowPropertyText(const char* label, float labelWidth, const char* value, float valueWidth, int id);

	static void TextRightAlignedPosX(float xpos, const char* fmt, ...);

	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

private:

	struct UITextureWindow
	{
		std::string title;
		alm::rhi::TextureHandle texture;
		bool opaque = false;
		bool redChannel = true;
		bool greenChannel = true;
		bool blueChannel = true;
		bool alphaChannel = false;
		float defaultImageWidth = 360.f;
		bool firstShow = true;
		int selectedMip = 0;
		int selectedSlice = 0;
	};

	struct RenderStageTextureView
	{
		alm::gfx::RenderStageTypeID renderStageId;
		alm::gfx::RenderGraph::AccessMode accessMode;
		std::string textureId;
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
		alm::gfx::RenderStageTypeID renderStageId;
		alm::gfx::RenderGraph::AccessMode accessMode;
		std::string bufferId;
		alm::gfx::RGBufferViewTicket ticket;
		std::unique_ptr<MemoryEditor> memEditor;
	};

private:

	void BuildMainMenu();
	void BuildBottomBar();

	void BuildSettingsWindow();
	void BuildSceneGraphWindow();
	void BuildRenderStagesWindow();

	void BuildRenderModesSettings();
	void BuildCameraSettings(float availWidth);
	void BuildDebugViewSettings(float availWidth);
	void BuildShadowmapSettings(float availWidth);
	void BuildDirectionalLightSettings(float availWidth);
	void BuildAmbientLightSettings(float availWidth);
	void BuildSkySettings(float availWidth);
	void BuildsCloudsSettings();
	void BuildMaterialChannelsSettings(float availWidth);
	void BuildSSAOSettings(float availWidth);
	void BuildBloomSettings(float availWidth);
	void BuildTonemappingSettings(float availWidth);

	void BuildLumninanceHistogram();

	void BuildTextureWindows();
	bool BuildTextureWindow(UITextureWindow& textureWindow);

	void BuildRSViews();
	bool BuildRSTexView(RenderStageTextureView* rsTexView);
	bool BuildRSBufferView(RenderStageBufferView* rsBufferView);

protected:

	SDL_Window* m_Window;
	alm::weak<alm::gfx::Scene> m_Scene;
	alm::weak<alm::gfx::RenderView> m_RenderViewUI;
	CameraController* m_CameraController;

private:

	float m_FPS = 0.f;
	float m_CPUTime = 0.f;
	float m_GPUTime = 0.f;

	bool m_ShowMainMenu = true;
	bool m_ShowBottomBar = true;

	bool m_ShowSettings = false;
	bool m_ShowSceneGraphWindow = false;
	bool m_ShowRenderStages = false;

	std::vector<RenderStageTextureView> m_RSTextureViews;
	std::vector<RenderStageBufferView> m_RSBufferViews;
	alm::gfx::RGTextureViewTicket m_RSTexViewFocus;
	alm::gfx::RGBufferViewTicket m_RSBufferViewFocus;

	std::vector<UITextureWindow> m_TextureWindows;

	alm::weak<alm::gfx::SceneGraphNode> m_SelectedNode;
	std::string m_RenderStageIOHoveredId;
	std::vector<std::string> m_BottomBarRightAlignTexts;

	int2 m_ShadowmapSizeCached = {-1, -1};

	bool m_ShowLuminanceHistogram = false;
	alm::gfx::RGBufferViewTicket m_LumHistogramBufferTicket;
	int m_LumHistogramMode = 0;

	std::vector<std::pair<std::string, const std::function<void()>>> m_MainMenuAdditionalItems;
};

} // namespace alm::fw