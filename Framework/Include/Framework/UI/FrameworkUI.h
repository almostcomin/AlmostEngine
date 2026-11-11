#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStages/SimpleSkyRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/AtmosSkyRenderStage.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/LightingMode.h"
#include "Gfx/AtmosphereConfig.h"
#include "Gfx/Scene.h"
#include "Gfx/RaycastHit.h"
#include "Gfx/TextureLoadFlags.h"
#include "Framework/UI/UIWindow.h"
#include "Framework/UI/SettingsWidgets.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace alm::gfx
{
	class SceneGraphNode;
	struct Material;
	class LoadedTexture;
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
			bool ShowHeightmapBBoxes = false;
		} Debug;

		bool ShadowmapEnabled = true;
		int2 ShadowmapSize = { 0, 0 };

		bool SunLightEnabled = true;
		alm::gfx::AtmosphereConfig::SunParams SunParams;
		bool SunParamsUpdated = false;

		alm::gfx::AtmosphereConfig::AmbientParams AmbientParams;
		bool AmbientParamsUpdated = false;

		alm::gfx::SimpleSkyRenderStage::SkyParams SimpleSkyParams;

		alm::gfx::AtmosphereConfig::SkyParams AtmosSkyParams;
		alm::gfx::AtmosSkyRenderStage::SimParams AtmosSkySimParams;

		alm::gfx::CloudsRenderStage::CloudsSimParams CloudsSimParams;
		uint32_t CloudsRTDenom = 0;
		bool CloudsEnabled = true;

		alm::gfx::SkySource SkySourceOption = alm::gfx::SkySource::Simple;
		bool CloudsShadowsEnabled = true;

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
			float MinLogLuminance = -10.f;
			float LogLuminanceRange = 12.f;
			float AdaptationUpSpeed = 2.f;
			float AdaptationDownSpeed = 0.5f;
			float MiddleGrayNits = 0.18f;
			float SdrExposureBias = 0.5f;
		} Exposure;

		struct
		{
			bool Enabled = true;
			float PaperWhiteNits = 203.f;
		} Tonemapping;

	} FrameworkData;

public:

	FrameworkUI();
	~FrameworkUI();

	virtual void Init(SDL_Window* window, weak<alm::gfx::Scene> scene, weak<alm::gfx::RenderView> renderView, CameraController* cameraController = nullptr);

	weak<alm::gfx::Scene> GetScene() const { return m_Scene; }

	// The render view the UI was initialized with (the one the panels inspect).
	alm::gfx::RenderView* GetUIRenderView() const { return m_RenderViewUI.get(); }

	CameraController* GetCameraController() const { return m_CameraController; }

	// Window registry: the UI windows are UIWindow subclasses (see UIWindows.h),
	// created by the FrameworkUI constructor and RegisterWindow() for app-specific
	// ones. They are drawn every frame by BuildUI and auto-listed in the View menu.
	template<class T>
	T* GetWindow()
	{
		for (const auto& window : m_Windows)
			if (T* result = dynamic_cast<T*>(window.get()))
				return result;
		return nullptr;
	}

	// Shared settings sections, shown by both the Settings and the Atmosphere windows.
	ShadowmapSettingsWidget& GetShadowmapWidget() { return m_ShadowmapWidget; }
	DirectionalLightWidget& GetDirectionalLightWidget() { return m_DirectionalLightWidget; }
	AmbientLightWidget& GetAmbientLightWidget() { return m_AmbientLightWidget; }

	void ShowBottomBar(bool b) { m_ShowBottomBar = b; }
	void AddBottomBarText(const std::string& text);

	void SetRenderStats(float fps, float cpuTime, float cpuIdleTime, float gpuTime);
	float GetFPS() const { return m_FPS; }
	float GetCPUTimeMs() const { return m_CPUTime; }
	float GetCPUIdleTimeMs() const { return m_CPUIdleTime; }
	float GetGPUTimeMs() const { return m_GPUTime; }

	void AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture);

	// Render stage IO inspection windows (delegates to the Render Stages window).
	void AddRenderStageTextureWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& textureId);
	void AddRenderStageBufferWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& bufferId);

	void RegisterMainMenuItem(const std::string& name, std::function<void()> item);

	void ShowErrorModal(const std::string& message);

	// Selects a scene graph node (delegates to the World Outliner window, which
	// owns the selection and syncs the ObjectOutlineRenderStage).
	void SetSelectedNode(const alm::weak<alm::gfx::SceneGraphNode>& node);

	// Selects the material and opens the Material Panel (e.g. clicking the material name button in the World Outliner)
	void OpenMaterialPanel(alm::gfx::Material* material);

	void SetGridVisible(bool b);
	bool IsGridVisible() const;

	void SwitchWireframeRender();

	std::optional<std::pair<uint2, uint2>> GetCentralViewport() const;

	std::function<void(const char*)> m_RequestLoadFile;
	std::function<void(const char*)> m_RequestMergeFile;
	std::function<void()> m_RequestClose;
	std::function<void()> m_RequestQuit;

	// ImGui helpers shared by the windows and the app UIs.
	static bool ShowToggleButton(const char* label, bool* v);
	static void ShowPropertyInt(const char* label, float labelWidth, int value, float valueWidth, int id);
	static void ShowPropertyText(const char* label, float labelWidth, const char* value, float valueWidth, int id);
	static void TextRightAlignedPosX(float xpos, const char* fmt, ...);

	// Texture show/browse/clear row, used by the Materials window and the app UIs.
	bool BuildTextureShowBrowseClear(const char* id, std::shared_ptr<alm::gfx::LoadedTexture>& loadedTex,
		alm::gfx::TextureLoadFlags loadFlags);
	bool BuildTextureShowBrowseClearPred(const char* id, std::shared_ptr<alm::gfx::LoadedTexture>& loadedTex,
		alm::gfx::TextureLoadFlags loadFlags, std::function<bool(const alm::gfx::LoadedTexture&)> pred);

	// Native file dialogs.
	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

	// ImGui image helper exposed to the windows (ShowImage is protected in ImGuiRenderStage).
	void ShowUIImage(alm::rhi::TextureHandle tex, alm::rhi::TextureSampledView view, const float2& size, const float2& uv0,
		const float2& uv1, int mip = 0, int slice = 0, ImGuiTexFlags flags = ImGuiTexFlags_None);

protected:

	void BuildUI() override;

	// Registers an app-specific window (call it from the app UI constructor). The
	// window is drawn after the framework ones and appended to the View menu
	// (unless it opts out of it). Returns the registered window.
	alm::fw::UIWindow* RegisterWindow(std::unique_ptr<UIWindow> window);

private:

	struct UITextureWindow
	{
		std::string title;
		alm::rhi::TextureHandle texture;
		alm::rhi::TextureSampledView arrayView;

		// ...
		bool opaque = false;
		bool redChannel = true;
		bool greenChannel = true;
		bool blueChannel = true;
		bool alphaChannel = false;
		float defaultImageWidth = 400.f;
		bool firstShow = true;
		int settleFrames = 4;
		int selectedMip = 0;
		int selectedSlice = 0;
	};

	void BuildMainMenu();
	void BuildBottomBar();

	void BuildGizmo(ImGuiID dockspaceId);

	void BuildTextureWindows();
	bool BuildTextureWindow(UITextureWindow& textureWindow);
	void ShowPendingErrorModal();

private:

	std::vector<std::unique_ptr<UIWindow>> m_Windows;

	ShadowmapSettingsWidget m_ShadowmapWidget;
	DirectionalLightWidget m_DirectionalLightWidget;
	AmbientLightWidget m_AmbientLightWidget;

	bool m_ShowMainMenu = true;
	bool m_ShowBottomBar = true;

	float m_FPS = 0.f;
	float m_CPUTime = 0.f;
	float m_CPUIdleTime = 0.f;
	float m_GPUTime = 0.f;

	std::string m_PendingErrorModal;  // Message shown by the "Error" modal (empty = no modal)

	std::vector<UITextureWindow> m_TextureWindows;

	// Right-aligned texts shown by the bottom bar on the next frame (AddBottomBarText)
	std::vector<std::string> m_BottomBarRightAlignTexts;

	std::vector<std::pair<std::string, const std::function<void()>>> m_MainMenuAdditionalItems;

protected:

	SDL_Window* m_Window;
	alm::weak<alm::gfx::Scene> m_Scene;
	alm::weak<alm::gfx::RenderView> m_RenderViewUI;
	CameraController* m_CameraController;
};

} // namespace alm::fw
