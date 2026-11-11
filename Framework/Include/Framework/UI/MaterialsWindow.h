#pragma once

// "Materials Panel" window (View > Material Panel): material list with name
// filter on the left, material editor on the right. UIWindow subclass owned by
// FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"

#include <imgui/imgui.h>

namespace alm::gfx
{
	struct Material;
}

namespace alm::fw
{

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

} // namespace alm::fw
