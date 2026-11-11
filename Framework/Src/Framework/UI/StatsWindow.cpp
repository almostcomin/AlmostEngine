// "Statistics" window (View > Statistics): culling stats and the rendered
// workload per pass. The standalone graph windows opened from its buttons live
// in their own files.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/StatsWindow.h"
#include "Framework/UI/GPUCPUGraphWindow.h"
#include "Framework/UI/StagesGPUGraphWindow.h"
#include "Framework/UI/StagesCPUGraphWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"

void alm::fw::StatsWindow::Build()
{
    auto cullingRS = m_UI.GetUIRenderView()->GetRenderGraph()->GetRenderStage<alm::gfx::GPUCullingRenderStage>();

    // Culling stats lifecycle: request while the window is open, release on close
    if (cullingRS)
    {
        if (m_Open && !m_CullingStatsRequested)
        {
            cullingRS->RequestStats();
            m_CullingStatsRequested = true;
        }
        else if (!m_Open && m_CullingStatsRequested)
        {
            cullingRS->UnrequestStats();
            m_CullingStatsRequested = false;
        }
    }

    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Statistics", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    // Graph windows openers
    if (ImGui::Button("View CPU/GPU time graph", ImVec2(-1, 0)))
    {
        if (auto* graph = m_UI.GetWindow<GPUCPUGraphWindow>())
            graph->SetOpen(true);
    }
    if (ImGui::Button("View Stages GPU time graph", ImVec2(-1, 0)))
    {
        if (auto* graph = m_UI.GetWindow<StagesGPUGraphWindow>())
            graph->SetOpen(true);
    }
    if (ImGui::Button("View Stages CPU time graph", ImVec2(-1, 0)))
    {
        if (auto* graph = m_UI.GetWindow<StagesCPUGraphWindow>())
            graph->SetOpen(true);
    }

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
        alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
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
