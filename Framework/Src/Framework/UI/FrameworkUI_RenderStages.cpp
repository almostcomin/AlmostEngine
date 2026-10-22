// FrameworkUI "Render Stages" window (View > Render Stages) and the render-stage
// texture/buffer inspection windows opened from it.
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include <imgui/imgui_internal.h>   // ImGui::GetCurrentWindow() (BuildRSTexView)
#include <sstream>
#include "imgui_memory_editor.h"

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

        float width = 160.0f;
        float height = ImGui::GetTextLineHeightWithSpacing();
        ImVec2 pos = ImGui::GetCursorScreenPos();

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
        ImGui::SameLine(startX + 90.f + width + 8.0f);
        ret.clicked = ImGui::Button("...", ImVec2(24, 0));

        ImGui::PopID();
        return ret;
    }
} // anonymous namespace

void alm::fw::FrameworkUI::BuildRenderStagesWindow()
{
    if (!m_ShowRenderStages)
        return;

    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();
    const float parentHeight = ImGui::GetIO().DisplaySize.y;
    const float windowHeight = parentHeight * 0.9f;

    ImGui::SetNextWindowSize(ImVec2(320, windowHeight), ImGuiCond_Once);
    std::string title = m_RenderViewUI->GetName() + " - " + renderGraph->GetCurrentRenderMode() + "###RenderViewWindow";
    if (!ImGui::Begin(title.c_str(), &m_ShowRenderStages, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    int propid = 0;
    std::string newHoveredId;
    for (int rs_idx = 0; rs_idx < renderGraph->GetNumRenderStages(); ++rs_idx)
    {
        const auto* rs = renderGraph->GetRenderStageDataFromIndex(rs_idx);
        if (ImGui::CollapsingHeader(rs->renderStage->GetDebugName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto addDep = [this, &newHoveredId, &propid, rs]<typename T>(const std::vector<T>&deps, bool isWrite, bool isBuffer)
            {
                for (const auto& dep : deps)
                {
                    const std::string& id = m_RenderViewUI->GetRenderGraph()->GetId(dep.handle);
                    bool selected = (m_RenderStageIOHoveredId == id);

                    RSDepActionResult result = ShowRSDep(isBuffer ? "BUFFER" : "TEXTURE", id.c_str(), selected, propid++);
                    if (result.hovered)
                    {
                        newHoveredId = id;
                    }
                    if (result.clicked)
                    {
                        if (isBuffer)
                        {
                            AddRenderStageBufferWindow(
                                rs->renderStage->GetType(),
                                isWrite ? alm::gfx::RenderGraph::AccessMode::Write : alm::gfx::RenderGraph::AccessMode::Read,
                                id);
                        }
                        else
                        {
                            AddRenderStageTextureWindow(
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
    m_RenderStageIOHoveredId = newHoveredId;

    ImGui::End();
}

void alm::fw::FrameworkUI::BuildRSViews()
{
    for (auto it = m_RSTextureViews.begin(); it != m_RSTextureViews.end();)
    {
        if (m_RSTexViewFocus == it->ticket)
        {
            ImGui::SetNextWindowFocus();
        }

        if (BuildRSTexView(&(*it)))
        {
            it++;
        }
        else
        {
            m_RenderViewUI->GetRenderGraph()->ReleaseTextureView(it->ticket);
            it = m_RSTextureViews.erase(it);
        }
    }

    for (auto it = m_RSBufferViews.begin(); it != m_RSBufferViews.end();)
    {
        if (m_RSBufferViewFocus == it->ticket)
        {
            ImGui::SetNextWindowFocus();
        }

        if (BuildRSBufferView(&(*it)))
        {
            it++;
        }
        else
        {
            m_RenderViewUI->GetRenderGraph()->ReleaseBufferView(it->ticket);
            it = m_RSBufferViews.erase(it);
        }
    }

    m_RSTexViewFocus = { nullptr };
    m_RSBufferViewFocus = { nullptr };
}

bool alm::fw::FrameworkUI::BuildRSTexView(RenderStageTextureView* rsTexView)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();
    bool isOpen = true;

    auto renderStage = m_RenderViewUI->GetRenderGraph()->GetRenderStage(rsTexView->renderStageId);
    if (!renderStage)
        return isOpen;

    alm::rhi::TextureHandle tex = renderGraph->GetTextureView(rsTexView->ticket);
    if (!tex)
        return isOpen;

    std::stringstream title;
    title << renderStage->GetDebugName() << " - ";
    if (rsTexView->accessMode == alm::gfx::RenderGraph::AccessMode::Read)
        title << "Read";
    else
        title << "Write";

    title << " - " << rsTexView->textureId;

    const auto& texDesc = tex->GetDesc();
    ImVec2 imageSize{ rsTexView->defaultImageWidth, rsTexView->defaultImageWidth * texDesc.height / texDesc.width };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float detailsHeight = ImGui::GetTextLineHeightWithSpacing() * 8.0f + ImGui::GetFrameHeightWithSpacing();
    const float childBorder = style.ChildBorderSize * 2.0f;
    const float spacingY = style.ItemSpacing.y;
    const float scrollbar = style.ScrollbarSize;
    const float windowWidth = imageSize.x + style.WindowPadding.x * 2.0f + style.WindowBorderSize * 2.0f + childBorder * 2 + 1.0f + 40.f;
    const float windowHeight = detailsHeight + spacingY + imageSize.y + style.WindowPadding.y * 2.0f + style.WindowBorderSize *
        2.0f + childBorder + scrollbar + 1.0f + 40.f;

    ImGui::SetNextWindowSize(ImVec2{ windowWidth, windowHeight }, ImGuiCond_Appearing);
    if (!ImGui::Begin(title.str().c_str(), &isOpen, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return isOpen;
    }

    if (!isOpen)
    {
        ImGui::End();
        return isOpen;
    }

    ImGuiTexFlags texFlags = 0;
    bool fitPressed = false;

    //
    // Detais child
    //

    ImGui::BeginChild("##details", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    {
        int id = 0;
        const float labelWidth = 80.f;
        const float labelHeight = 120.f;

        ShowPropertyInt("Width", labelWidth, texDesc.width, labelHeight, ++id);
        ShowPropertyInt("Height", labelWidth, texDesc.height, labelHeight, ++id);
        ShowPropertyInt("Depth", labelWidth, texDesc.depth, labelHeight, ++id);
        ShowPropertyInt("ArraySize", labelWidth, texDesc.arraySize, labelHeight, ++id);
        ShowPropertyInt("MipLevels", labelWidth, texDesc.mipLevels, labelHeight, ++id);
        ShowPropertyText("Format", labelWidth, alm::rhi::GetFormatString(texDesc.format), labelHeight, ++id);

        fitPressed = ImGui::Button("Fit");
        ImGui::SameLine();
        ShowToggleButton("Alpha", &rsTexView->applyAlpha);
        ImGui::SameLine();
        if (ShowToggleButton("R", &rsTexView->redChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (ShowToggleButton("G", &rsTexView->greenChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (ShowToggleButton("B", &rsTexView->blueChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (ShowToggleButton("A", &rsTexView->alphaChannel))
        {
            rsTexView->redChannel = false;
            rsTexView->greenChannel = false;
            rsTexView->blueChannel = false;
        }

        if (!rsTexView->applyAlpha)
            texFlags |= ImGuiTexFlags_IgnoreAlpha;
        if (!rsTexView->redChannel)
            texFlags |= ImGuiTexFlags_HideRedChannel;
        if (!rsTexView->greenChannel)
            texFlags |= ImGuiTexFlags_HideGreenChannel;
        if (!rsTexView->blueChannel)
            texFlags |= ImGuiTexFlags_HideBlueChannel;
        if (rsTexView->alphaChannel)
            texFlags |= ImGuiTexFlags_ShowAlphaChannel;
    }
    ImGui::EndChild();

    //
    // Image child
    //

    ImGui::SetNextWindowContentSize(imageSize);
    ImGuiWindowFlags imageChildFlags = ImGuiWindowFlags_None;
    if (rsTexView->firstShow)
    {
        imageChildFlags |= ImGuiWindowFlags_NoScrollbar;
    }
    else
    {
        imageChildFlags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    ImVec2 childSize{ 0.f, 0.f };
    if (rsTexView->firstShow)
    {
        childSize = { imageSize.x, imageSize.y };
    }

    ImGui::BeginChild("##image", childSize, ImGuiChildFlags_None, imageChildFlags);
    {
        ImRect clip = ImGui::GetCurrentWindow()->ClipRect;
        ImVec2 avail = clip.GetSize();//ImGui::GetContentRegionAvail();

        if (rsTexView->firstShow)
        {
            float scale = (std::max)((float)texDesc.width / avail.x, (float)texDesc.height / avail.y);
            rsTexView->defaultImageWidth = texDesc.width / scale - 1.f;
        }
        else
        {
            if (fitPressed)
            {
                float scale = (std::max)((float)texDesc.width / avail.x, (float)texDesc.height / avail.y);
                rsTexView->defaultImageWidth = texDesc.width / scale - 1.f;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ShowImage(tex, { imageSize.x, imageSize.y }, { 0.f , 0.f }, { 1.f, 1.f }, 0, 0, texFlags);
            ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    rsTexView->firstShow = false;
    return isOpen;
}

bool alm::fw::FrameworkUI::BuildRSBufferView(RenderStageBufferView* rsBufferView)
{
    bool isOpen = true;

    auto renderStage = m_RenderViewUI->GetRenderGraph()->GetRenderStage(rsBufferView->renderStageId);
    if (!renderStage)
        return isOpen;

    alm::rhi::BufferHandle buffer = m_RenderViewUI->GetRenderGraph()->GetBufferView(rsBufferView->ticket);
    if (!buffer)
        return isOpen;

    std::stringstream title;
    title << renderStage->GetDebugName() << " - ";
    if (rsBufferView->accessMode == alm::gfx::RenderGraph::AccessMode::Read)
        title << "Read";
    else
        title << "Write";
    title << " - " << rsBufferView->bufferId.c_str();
    std::string titleStr = title.str();

    void* dataPtr = buffer->Map();
    rsBufferView->memEditor->DrawWindow(titleStr.c_str(), dataPtr, buffer->GetDesc().sizeBytes);
    buffer->Unmap();

    return rsBufferView->memEditor->Open;
}

