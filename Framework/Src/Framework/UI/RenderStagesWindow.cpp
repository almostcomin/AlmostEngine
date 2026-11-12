// "Render Stages" window (View > Render Stages). Opens the render-stage texture/
// buffer inspection windows (owned and built by FrameworkUI); each of them
// releases its render graph view ticket when closed.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/RenderStagesWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"

namespace
{

    struct RSDepActionResult
    {
        bool hovered;
        bool clicked;
    };

    RSDepActionResult ShowRSDep(const char* label, const char* value, bool selected, int id)
    {
        RSDepActionResult ret;

        ImGui::PushID(id);

        // Label
        float startX = ImGui::GetCursorPosX();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(90.0f);

        float height = ImGui::GetTextLineHeightWithSpacing();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionMax().x - 90.f - 36.f - ImGui::GetStyle().WindowPadding.x;// 160.0f;

        // hitbox
        ImGui::InvisibleButton("##select", ImVec2(width, height));
        ret.hovered = ImGui::IsItemHovered();

        // background
        ImU32 bg = ret.hovered ? ImGui::GetColorU32(ImGuiCol_HeaderHovered) : selected ? ImGui::GetColorU32(ImGuiCol_Header) : 0;
        if (bg)
            ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg, ImGui::GetStyle().FrameRounding);

        // Draw text
        ImGui::SetCursorScreenPos(ImVec2(pos.x + ImGui::GetStyle().FramePadding.x, pos.y));
        ImGui::TextUnformatted(value);

        // Button
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 36.0f);
        ret.clicked = ImGui::Button("...", ImVec2(36, 0));

        ImGui::PopID();
        return ret;
    }
} // anonymous namespace

void alm::fw::RenderStagesWindow::AddTextureView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& textureId)
{
    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();

    auto ticket = renderGraph->RequestTextureView(rsId, accessMode, renderGraph->GetTextureHandle(textureId));

    // Empty title: FrameworkUI derives it from the view texture debug name
    // ("stage - access mode - resource id"). The release lambda runs when the
    // inspection window is closed.
    auto graph = m_UI.GetUIRenderView()->GetRenderGraph();
    auto tex = renderGraph->GetTextureViewTexture(ticket);
    if (!tex || !m_UI.AddTextureWindow(tex, {}, [graph, ticket]
        {
            if (!graph.expired())
                graph->ReleaseTextureView(ticket);
        }))
    {
        // The window was already open (or the view texture creation failed):
        // drop the extra reference taken by RequestTextureView.
        renderGraph->ReleaseTextureView(ticket);
    }
}

void alm::fw::RenderStagesWindow::AddBufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& bufferId)
{
    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();

    auto ticket = renderGraph->RequestBufferView(rsId, accessMode, renderGraph->GetBufferHandle(bufferId));

    auto graph = m_UI.GetUIRenderView()->GetRenderGraph();
    auto buffer = renderGraph->GetBufferViewBuffer(ticket);
    if (!buffer || !m_UI.AddBufferWindow(buffer, {}, [graph, ticket]
        {
            if (!graph.expired())
                graph->ReleaseBufferView(ticket);
        }))
    {
        renderGraph->ReleaseBufferView(ticket);
    }
}

void alm::fw::RenderStagesWindow::Build()
{
    if (m_Open)
    {
        alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
        const float parentHeight = ImGui::GetIO().DisplaySize.y;
        const float windowHeight = parentHeight * 0.9f;

        ImGui::SetNextWindowSize(ImVec2(520, windowHeight), ImGuiCond_Once);
        std::string title = "Render Stgages: " + m_UI.GetUIRenderView()->GetName() + " - "
            + renderGraph->GetCurrentRenderMode() + "###RenderViewWindow";
        if (!ImGui::Begin(title.c_str(), &m_Open, ImGuiWindowFlags_None))
        {
            ImGui::End();
        }
        else
        {
            int propid = 0;
            std::string newHoveredId;
            for (int rs_idx = 0; rs_idx < renderGraph->GetNumRenderStages(); ++rs_idx)
            {
                const auto* rs = renderGraph->GetRenderStageDataFromIndex(rs_idx);
                if (ImGui::CollapsingHeader(rs->renderStage->GetDebugName(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto addDep = [this, renderGraph, &newHoveredId, &propid, rs]<typename T>(const std::vector<T>&deps, bool isWrite, bool isBuffer)
                    {
                        for (const auto& dep : deps)
                        {
                            const std::string& id = renderGraph->GetId(dep.handle);
                            bool selected = (m_IOHoveredId == id);

                            RSDepActionResult result = ShowRSDep(isBuffer ? "BUFFER" : "TEXTURE", id.c_str(), selected, propid++);
                            if (result.hovered)
                            {
                                newHoveredId = id;
                            }
                            if (result.clicked)
                            {
                                if (isBuffer)
                                {
                                    AddBufferView(
                                        rs->renderStage->GetType(),
                                        isWrite ? alm::gfx::RenderGraph::AccessMode::Write : alm::gfx::RenderGraph::AccessMode::Read,
                                        id);
                                }
                                else
                                {
                                    AddTextureView(
                                        rs->renderStage->GetType(),
                                        isWrite ? alm::gfx::RenderGraph::AccessMode::Write : alm::gfx::RenderGraph::AccessMode::Read,
                                        id);
                                }
                            }
                        }
                    };

                    if (!rs->textureReads.empty() || !rs->bufferReads.empty())
                    {
                        ImGui::SeparatorText("Reads");
                        addDep(rs->textureReads, false, false);
                        addDep(rs->bufferReads, false, true);
                    }

                    if (!rs->textureWrites.empty() || !rs->bufferWrites.empty())
                    {
                        ImGui::SeparatorText("Writes");
                        addDep(rs->textureWrites, true, false);
                        addDep(rs->bufferWrites, true, true);
                    }

                    ImGui::SeparatorText("Stats");
                    {
                        float gpuTimeMs = renderGraph->GetStageGPUTimeMs(rs->renderStage->GetType());
                        float cpuTimeMs = renderGraph->GetStageCPUTimeMs(rs->renderStage->GetType());

                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);

                        ImGui::Text("CPU time: %1.3fms", cpuTimeMs);
                        ImGui::SameLine();
                        TextRightAligned("GPU time: %1.3fms", gpuTimeMs);

                        ImGui::PopStyleColor();
                    }
                }
            }
            m_IOHoveredId = newHoveredId;

            ImGui::End();
        }
    }
}
