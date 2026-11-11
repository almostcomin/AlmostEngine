#pragma once

// Concrete UIWindow subclasses of the FrameworkUI window set. Their member
// functions are implemented in the FrameworkUI_*.cpp files, one per window
// (same file layout as before the UIWindow refactor). FrameworkUI owns one
// instance of each, created in its constructor (see FrameworkUI.cpp).

#include "Framework/UI/UIWindow.h"
#include "Framework/UI/TimePlot.h"
#include "Framework/UI/CloudsPresets.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Core/Memory.h"

#include <imgui/imgui.h>

#include <memory>
#include <string>
#include <vector>

struct MemoryEditor;

namespace alm::gfx
{
	struct Material;
	class SceneGraphNode;
}

namespace alm::fw
{

class FrameworkUI;

// ---------------------------------------------------------------- Settings

// "Settings" window (View > Settings).
class SettingsWindow : public UIWindow
{
public:

	explicit SettingsWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Settings"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_S; }
	void Build() override;

private:

	void BuildRenderModesSettings();
	void BuildCameraSettings(float availWidth);
	void BuildDebugViewSettings(float availWidth);
	void BuildMaterialChannelsSettings(float availWidth);
	void BuildSSAOSettings(float availWidth);
	void BuildBloomSettings(float availWidth);
	void BuildExposureSettings(float availWidth);
	void BuildTonemappingSettings(float availWidth);
};

// "Luminance histogram" window, opened from the Exposure section of Settings.
class LuminanceHistogramWindow : public UIWindow
{
public:

	explicit LuminanceHistogramWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Luminance histogram"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::gfx::RGBufferViewTicket m_BufferTicket;
	int m_Mode = 0;   // 0: log2 scale, 1: linear scale
};

// -------------------------------------------------------- Atmosphere/Lighting

// "Atmosphere / Lighting" window (View > Atmosphere / Lighting).
class AtmosphereWindow : public UIWindow
{
public:

	explicit AtmosphereWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Atmosphere / Lighting"; }
	void Build() override;

private:

	void BuildSimpleSkySettings();
	void BuildSimSkySettings();
	void BuildCloudsSettings();
	void BuildCubemapSettings(float availWidth);

	CloudsPreset m_CloudsPreset = CloudsPreset::FairWeather;
	alm::gfx::CloudsRenderStage::DebugChannel m_CloudsDebugChannel = alm::gfx::CloudsRenderStage::DebugChannel::Disabled;
};

// ------------------------------------------------------------ World Outliner

// "World Outliner" window (View > World Outliner), including the 3D viewport
// node picking and the per-leaf-type inspector sections. It also owns the node
// selection: the selection input (validity, ESC/Delete shortcuts, viewport
// picking) runs every frame, whether the window is open or not.
class WorldOutlinerWindow : public UIWindow
{
public:

	explicit WorldOutlinerWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "World Outliner"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_W; }
	void Build() override;

	// Selection input, built every frame by FrameworkUI::BuildUI regardless of
	// the window being open: node validity, ESC/Delete shortcuts and picking.
	void BuildSelectionInput();

	void SetSelectedNode(const alm::weak<alm::gfx::SceneGraphNode>& node);
	const alm::weak<alm::gfx::SceneGraphNode>& GetSelectedNode() const { return m_SelectedNode; }

	// Opens the window focusing the selected node (expands its ancestors and
	// scrolls the tree to it). Re-focuses even if the window is already open.
	void OpenWithSelectedNodeFocus();

protected:

	void OnOpen() override { FocusSelectedNode(); }

private:

	void BuildViewportPicking();

	// Expands the ancestors of the selected node and scrolls the tree to it
	// (no-op without a live selection). Does not change the open state.
	void FocusSelectedNode();

	alm::weak<alm::gfx::SceneGraphNode> m_SelectedNode;
	bool m_ScrollToSelectedNode = false;
	std::vector<const alm::gfx::SceneGraphNode*> m_ExpandAncestors;
};

// ------------------------------------------------------------- Render Stages

// "Render Stages" window (View > Render Stages) and the render-stage texture/
// buffer inspection windows opened from it (those outlive the main window).
class RenderStagesWindow : public UIWindow
{
public:

	explicit RenderStagesWindow(FrameworkUI& ui) : UIWindow(ui) {}

	// Defined out-of-line in FrameworkUI_RenderStages.cpp: the BufferView views hold a
	// MemoryEditor whose complete type is only visible there.
	~RenderStagesWindow() override;

	const char* GetName() const override { return "Render Stages"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_R; }
	void Build() override;

	// Opens (or focuses) the inspection window of a render stage texture/buffer IO.
	void AddTextureView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& textureId);
	void AddBufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& bufferId);

private:

	struct TextureView
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

	// NOTE: BufferView has out-of-line special members (defined in
	// FrameworkUI_RenderStages.cpp) because it owns a MemoryEditor whose complete
	// type is only visible there: an implicitly-generated destructor would fail to
	// compile ("can't delete an incomplete type") in every other window TU.
	struct BufferView
	{
		BufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
			const std::string& bufferId, alm::gfx::RGBufferViewTicket ticket);
		~BufferView();
		BufferView(BufferView&& other) noexcept;
		BufferView& operator=(BufferView&& other) noexcept;

		alm::gfx::RenderStageTypeID renderStageId;
		alm::gfx::RenderGraph::AccessMode accessMode;
		std::string bufferId;
		alm::gfx::RGBufferViewTicket ticket;
		std::unique_ptr<MemoryEditor> memEditor;
	};

	void BuildViews();
	bool BuildTextureView(TextureView* rsTexView);
	bool BuildBufferView(BufferView* rsBufferView);

	std::vector<TextureView> m_TextureViews;
	std::vector<BufferView> m_BufferViews;
	alm::gfx::RGTextureViewTicket m_TexViewFocus;
	alm::gfx::RGBufferViewTicket m_BufferViewFocus;
	std::string m_IOHoveredId;
};

// ------------------------------------------------------------ Material Panel

// "Materials Panel" window (View > Material Panel).
class MaterialsWindow : public UIWindow
{
public:

	explicit MaterialsWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Material Panel"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_M; }
	void Build() override;

	// Selects the material and opens the panel (e.g. clicking the material name
	// button in the World Outliner).
	void SelectMaterial(alm::gfx::Material* material);

private:

	alm::gfx::Material* m_SelectedMaterial = nullptr;
	bool m_ScrollToSelectedMaterial = false;
	ImGuiTextFilter m_Filter;
};

// -------------------------------------------------------------- Statistics

// "Statistics" window (View > Statistics).
class StatsWindow : public UIWindow
{
public:

	explicit StatsWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Statistics"; }
	void Build() override;

private:

	// Whether this window holds a GPUCullingRenderStage stats request
	// (RequestStats/UnrequestStats lifecycle follows the window being open)
	bool m_CullingStatsRequested = false;
};

// Standalone graph windows opened from the Statistics window buttons.
class GPUCPUGraphWindow : public UIWindow
{
public:

	explicit GPUCPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "CPU/GPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

class StagesGPUGraphWindow : public UIWindow
{
public:

	explicit StagesGPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Stages GPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

class StagesCPUGraphWindow : public UIWindow
{
public:

	explicit StagesCPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Stages CPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

// ------------------------------------------------------------ ImGui Debugger

// ImGui Metrics/Debugger window (window list, draw commands, input routing).
class ImGuiDebuggerWindow : public UIWindow
{
public:

	explicit ImGuiDebuggerWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "ImGui Debugger"; }
	void Build() override;
};

} // namespace alm::fw
