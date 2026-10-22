// FrameworkUI "Statistics" window (View > Statistics) and the standalone graph
// windows opened from it.
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"

namespace
{

    // BuildStatsWindow: time plot series colors
    const ImVec4 s_GPULineColor(0.0f, 0.45f, 1.0f, 1.0f);
    const ImVec4 s_CPULineColor(0.25f, 1.0f, 0.35f, 1.0f);
} // anonymous namespace

void alm::fw::FrameworkUI::BuildStatsWindow()
{    
    auto cullingRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::GPUCullingRenderStage>();

    // Culling stats lifecycle: request while the window is open, release on close
    if (cullingRS)
    {
        if (m_ShowStats && !m_StatsRequestedByUI)
        {
            cullingRS->RequestStats();
            m_StatsRequestedByUI = true;
        }
        else if (!m_ShowStats && m_StatsRequestedByUI)
        {
            cullingRS->UnrequestStats();
            m_StatsRequestedByUI = false;
        }
    }

    if (!m_ShowStats)
        return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Statistics", &m_ShowStats, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    // Graph windows openers
    if (ImGui::Button("View CPU/GPU time graph", ImVec2(-1, 0)))
        m_ShowGPUCPUGraph = true;
    if (ImGui::Button("View Stages GPU time graph", ImVec2(-1, 0)))
        m_ShowStagesGPUGraph = true;
    if (ImGui::Button("View Stages CPU time graph", ImVec2(-1, 0)))
        m_ShowStagesCPUGraph = true;

    ImGui::Spacing();

    // Culling stats
    const interop::CullingStats* cullingStats = nullptr;
    ImGui::SeparatorText("Culling stats");
    {
        if (!cullingRS)
        {
            ImGui::TextDisabled("No culling stage in the render graph");
        }
        else if (!cullingRS->IsStatsRequested())
        {
            ImGui::TextDisabled("No culling stats requested");
        }
        else
        {
            cullingStats = cullingRS->GetStats();
            assert(cullingStats);
            auto culledPercentage = [](uint32_t total, uint32_t visible)
                {
                    return total > 0 ? 100.f * float(total - visible) / float(total) : 0.f;
                };

            // Aggregate the per-bucket stats over all buckets (all domains and cull faces)
            interop::CullingBucketStats totals{};
            for (const auto& bucketStats : cullingStats->Buckets)
            {
                totals.CameraTotalInstances += bucketStats.CameraTotalInstances;
                totals.CameraVisibleInstances += bucketStats.CameraVisibleInstances;
                totals.CameraTotalPrimitives += bucketStats.CameraTotalPrimitives;
                totals.CameraVisiblePrimitives += bucketStats.CameraVisiblePrimitives;
                totals.ShadowTotalInstances += bucketStats.ShadowTotalInstances;
                totals.ShadowVisibleInstances += bucketStats.ShadowVisibleInstances;
                totals.ShadowTotalPrimitives += bucketStats.ShadowTotalPrimitives;
                totals.ShadowVisiblePrimitives += bucketStats.ShadowVisiblePrimitives;
            }

            ImGui::Text("Camera frustum");
            ImGui::Text("  Instances:  %u / %u  (%.1f%% culled)",
                totals.CameraVisibleInstances, totals.CameraTotalInstances,
                culledPercentage(totals.CameraTotalInstances, totals.CameraVisibleInstances));
            ImGui::Text("  Primitives: %u / %u  (%.1f%% culled)",
                totals.CameraVisiblePrimitives, totals.CameraTotalPrimitives,
                culledPercentage(totals.CameraTotalPrimitives, totals.CameraVisiblePrimitives));

            ImGui::Spacing();

            ImGui::Text("Shadow map");
            ImGui::Text("  Instances:  %u / %u  (%.1f%% culled)",
                totals.ShadowVisibleInstances, totals.ShadowTotalInstances,
                culledPercentage(totals.ShadowTotalInstances, totals.ShadowVisibleInstances));
            ImGui::Text("  Primitives: %u / %u  (%.1f%% culled)",
                totals.ShadowVisiblePrimitives, totals.ShadowTotalPrimitives,
                culledPercentage(totals.ShadowTotalPrimitives, totals.ShadowVisiblePrimitives));
        }
    }

    // Rendered workload per pass: only the stages of the current render mode.
    // nullopt = not a pass stage (skipped); a valid result (zeros for disabled
    // stages) is the workload that pass submits.
    ImGui::SeparatorText("Draw stats");
    {
        alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();
        const std::string& currentMode = renderGraph->GetCurrentRenderMode();

        if (ImGui::BeginTable("##RenderedPerPass", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Instances");
            ImGui::TableSetupColumn("Triangles");
            ImGui::TableSetupColumn("Millions");
            ImGui::TableHeadersRow();

            uint32_t totalInstances = 0;
            uint32_t totalPrimitives = 0;
            for (size_t i = 0; i < renderGraph->GetNumRenderStages(currentMode); ++i)
            {
                const auto* rsData = renderGraph->GetRenderStageDataFromIndex((uint32_t)i, currentMode);
                if (!rsData || !rsData->renderStage)
                    continue;

                const auto passStats = rsData->renderStage->GetRenderedStats(cullingStats);
                if (!passStats)
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(rsData->renderStage->GetDebugName());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", passStats->VisibleInstances);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", passStats->VisiblePrimitives);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2fM", passStats->VisiblePrimitives / 1e6f);

                totalInstances += passStats->VisibleInstances;
                totalPrimitives += passStats->VisiblePrimitives;
            }

            // Highlighted total row (same background as the table headers)
            ImGui::TableNextRow();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Total");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", totalInstances);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", totalPrimitives);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2fM", totalPrimitives / 1e6f);

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

void alm::fw::FrameworkUI::BuildGPUCPUGraphWindow()
{
    if (!m_ShowGPUCPUGraph)
        return;

    // Register the series once: each line is fed by a functor sampled on Update
    if (!m_TimePlotReady)
    {
        m_TimePlot.AddLine("GPU ms", s_GPULineColor, [this] { return m_GPUTime; });
        m_TimePlot.AddLine("CPU ms", s_CPULineColor, [this] { return m_CPUTime - m_CPUIdleTime; });
        m_TimePlot.SetBottomMargin(ImGui::GetStyle().FramePadding.y * 2.f);
        m_TimePlotReady = true;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_Once);
    if (!ImGui::Begin("CPU/GPU time graph", &m_ShowGPUCPUGraph, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    m_TimePlot.Update();
    ImGui::End();
}

void alm::fw::FrameworkUI::BuildStagesGPUGraphWindow()
{
    if (!m_ShowStagesGPUGraph)
        return;

    auto* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    // GPU times of every render stage in the graph (fed by the stages' timer queries)
    if (!m_StagesGPUPlotReady)
    {
        const size_t numStages = renderGraph->GetNumRenderStages();
        for (size_t i = 0; i < numStages; ++i)
        {
            const auto* rs = renderGraph->GetRenderStageDataFromIndex((uint32_t)i);
            if (!rs || !rs->renderStage)
                continue;

            const auto stageType = rs->renderStage->GetType();
            m_StagesGPUPlot.AddLine(rs->renderStage->GetDebugName(),
                [renderGraph, stageType] { return renderGraph->GetStageGPUTimeMs(stageType); });
        }
        m_StagesGPUPlot.SetBottomMargin(ImGui::GetStyle().FramePadding.y * 2.f);
        m_StagesGPUPlot.SetMinY(0.01f);
        m_StagesGPUPlotReady = true;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_Once);
    if (!ImGui::Begin("Stages GPU time graph", &m_ShowStagesGPUGraph, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    m_StagesGPUPlot.Update();
    ImGui::End();
}

void alm::fw::FrameworkUI::BuildStagesCPUGraphWindow()
{
    if (!m_ShowStagesCPUGraph)
        return;

    auto* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    // CPU times of every render stage in the graph (fed by the stages' timer queries)
    if (!m_StagesCPUPlotReady)
    {
        const size_t numStages = renderGraph->GetNumRenderStages();
        for (size_t i = 0; i < numStages; ++i)
        {
            const auto* rs = renderGraph->GetRenderStageDataFromIndex((uint32_t)i);
            if (!rs || !rs->renderStage)
                continue;

            const auto stageType = rs->renderStage->GetType();
            m_StagesCPUPlot.AddLine(rs->renderStage->GetDebugName(),
                [renderGraph, stageType] { return renderGraph->GetStageCPUTimeMs(stageType); });
        }
        m_StagesCPUPlot.SetBottomMargin(ImGui::GetStyle().FramePadding.y * 2.f);
        m_StagesCPUPlot.SetMinY(0.01f);
        m_StagesCPUPlotReady = true;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_Once);
    if (!ImGui::Begin("Stages CPU time graph", &m_ShowStagesCPUGraph, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    m_StagesCPUPlot.Update();
    ImGui::End();
}

