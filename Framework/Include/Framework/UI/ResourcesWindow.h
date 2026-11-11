#pragma once

#include "Framework/UI/UIWindow.h"

#include <imgui/imgui.h>
#include <cstdint>

namespace alm::rhi
{
	class IResource;
}

namespace alm::fw
{

class ResourcesWindow : public UIWindow
{
public:

	explicit ResourcesWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Resources Inspector"; }
	ImGuiKeyChord GetOpenHotkey() const override { return ImGuiKey_None; }
	void Build() override;

private:

	// Opens (or focuses) the inspection window of a texture/buffer resource.
	void Inspect(rhi::IResource* res);

	ImGuiTextFilter m_Filter;
	uint32_t m_VisibleTypeMask = ~0u; // Bit (size_t)ResourceType set -> resources of that type are listed
	alm::rhi::IResource* m_SelectedResource = nullptr;
	bool m_ScrollToSelectedResource = false;
};

} // namespace alm::fw
