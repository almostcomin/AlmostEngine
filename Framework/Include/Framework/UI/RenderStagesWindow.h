#pragma once

// "Render Stages" window (View > Render Stages) and the render-stage texture/
// buffer inspection windows opened from it (those outlive the main window).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderGraphTypes.h"

#include <memory>
#include <string>
#include <vector>

struct MemoryEditor;

namespace alm::fw
{

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

} // namespace alm::fw
