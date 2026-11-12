#pragma once

// "Render Stages" window (View > Render Stages). Opens the render-stage texture/
// buffer inspection windows, which are owned and built by FrameworkUI (their
// lifetime is independent of this window's). UIWindow subclass owned by
// FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStageType.h"

#include <string>

namespace alm::fw
{

class RenderStagesWindow : public UIWindow
{
public:

	explicit RenderStagesWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Render Stages"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiMod_Ctrl | ImGuiKey_R; }
	void Build() override;

	// Opens (or focuses) the inspection window of a render stage texture/buffer IO.
	// The inspection window owns the render graph view ticket and releases it when closed.
	void AddTextureView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& textureId);
	void AddBufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& bufferId);

private:

	std::string m_IOHoveredId;
};

} // namespace alm::fw
