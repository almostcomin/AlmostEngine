// "Stages GPU time graph" window: GPU time of every render stage over time,
// opened from the Statistics window buttons.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/StagesGPUGraphWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"

void alm::fw::StagesGPUGraphWindow::Build()
{
    if (!m_Open)
        return;

    auto* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();

    // GPU times of every render stage in the graph (fed by the stages' timer queries)
    if (!m_PlotReady)
    {
        const size_t numStages = renderGraph->GetNumRenderStages();
        for (size_t i = 0; i < numStages; ++i)
        {
            const auto* rs = renderGraph->GetRenderStageDataFromIndex((uint32_t)i);
            if (!rs || !rs->renderStage)
                continue;

            const auto stageType = rs->renderStage->GetType();
            m_Plot.AddLine(rs->renderStage->GetDebugName(),
                [renderGraph, stageType] { return renderGraph->GetStageGPUTimeMs(stageType); });
        }
        m_Plot.SetBottomMargin(ImGui::GetStyle().FramePadding.y * 2.f);
        m_Plot.SetMinY(0.01f);
        m_PlotReady = true;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_Once);
    if (!ImGui::Begin("Stages GPU time graph", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    m_Plot.Update();
    ImGui::End();
}
