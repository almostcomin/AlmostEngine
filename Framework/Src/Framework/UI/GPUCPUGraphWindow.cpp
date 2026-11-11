// "CPU/GPU time graph" window: frame CPU/GPU times over time, opened from the
// Statistics window buttons.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/GPUCPUGraphWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include <imgui/imgui.h>

namespace
{

    // Graph series colors
    const ImVec4 s_GPULineColor(0.0f, 0.45f, 1.0f, 1.0f);
    const ImVec4 s_CPULineColor(0.25f, 1.0f, 0.35f, 1.0f);
} // anonymous namespace

void alm::fw::GPUCPUGraphWindow::Build()
{
    if (!m_Open)
        return;

    // Register the series once: each line is fed by a functor sampled on Update
    if (!m_PlotReady)
    {
        m_Plot.AddLine("GPU ms", s_GPULineColor, [this] { return m_UI.GetGPUTimeMs(); });
        m_Plot.AddLine("CPU ms", s_CPULineColor, [this] { return m_UI.GetCPUTimeMs() - m_UI.GetCPUIdleTimeMs(); });
        m_Plot.SetBottomMargin(ImGui::GetStyle().FramePadding.y * 2.f);
        m_PlotReady = true;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_Once);
    if (!ImGui::Begin("CPU/GPU time graph", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    m_Plot.Update();
    ImGui::End();
}
