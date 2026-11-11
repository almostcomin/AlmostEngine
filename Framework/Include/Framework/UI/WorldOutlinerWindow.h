#pragma once

// "World Outliner" window (View > World Outliner), including the 3D viewport
// node picking and the per-leaf-type inspector sections. It also owns the node
// selection: the selection input (validity, ESC/Delete shortcuts, viewport
// picking) runs every frame, whether the window is open or not.
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Core/Memory.h"

#include <vector>

namespace alm::gfx
{
	class SceneGraphNode;
}

namespace alm::fw
{

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

} // namespace alm::fw
