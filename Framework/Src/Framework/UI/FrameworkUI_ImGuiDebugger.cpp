// "ImGui Debugger" window (View > ImGui Debugger): wraps ImGui's own
// Metrics/Debugger window (window list, draw commands, input routing).

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/ImGuiDebuggerWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include <imgui/imgui.h>

void alm::fw::ImGuiDebuggerWindow::Build()
{
    if (!m_Open)
        return;

    ImGui::ShowMetricsWindow(&m_Open);
}
