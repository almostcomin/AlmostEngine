#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderView.h"
#include <functional>
#include "Core/Memory.h"

namespace st::gfx
{
	class SceneGraphNode;
	class MeshInstance;
	class DeviceManager;
	class ShadowmapRenderStage;
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
		float CameraSpeed = 2.f;
		bool ShowBBoxes = false;
		bool ShadowmapEnabled = true;
		int2 ShadowmapSize;
		float AmbientColorIntensity;
		float3 SkyColor;
		float3 GroundColor;
	};

	StructureUI(st::weak<st::gfx::RenderView> renderView, SDL_Window* window, st::gfx::ShadowmapRenderStage* shadowmapRS, st::gfx::DeviceManager* deviceManager);

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
		st::gfx::RenderView::AccessMode accessMode;
		std::string id;
		st::gfx::RenderView::TextureViewTicket ticket;
		bool applyAlpha = false;
		bool redChannel = true;
		bool greenChannel = true;
		bool blueChannel = true;
		bool alphaChannel = false;
		float defaultImageWidth = 360.f;
		bool firstShow = true;
	};

private:

	void BuildMainMenu();
	void BuildBottomBar();
	void BuildMenuFile();

	void BuildSettingsWindow();
	void BuildSceneWindow(bool* p_open);
	void BuildResourcesWindow(bool* p_open);
	void BuildRenderStagesWindow();
	bool BuildRSTexView(RenderStageTextureView* rsTexView);

	void BuildMeshInstanceLeaf(const st::gfx::MeshInstance* leaf);

	void AddRenderStageTextureView(st::gfx::RenderStage* renderStage, st::gfx::RenderView::AccessMode accessMode, const std::string& id);

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
};