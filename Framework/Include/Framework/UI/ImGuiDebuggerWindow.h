#pragma once

// ImGui Metrics/Debugger window (window list, draw commands, input routing).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"

namespace alm::fw
{

class ImGuiDebuggerWindow : public UIWindow
{
public:

	explicit ImGuiDebuggerWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "ImGui Debugger"; }
	void Build() override;
};

} // namespace alm::fw
