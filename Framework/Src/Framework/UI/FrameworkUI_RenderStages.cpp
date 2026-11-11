// "Render Stages" window (View > Render Stages) and the render-stage texture/
// buffer inspection windows opened from it. UIWindow subclass of the FrameworkUI
// window set (see Framework/UI/UIWindows.h). The inspection windows outlive the
// main window: they are built while any of them is open, even if the main
// window is closed.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/UIWindows.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include <imgui/imgui_internal.h>   // ImGui::GetCurrentWindow() (BuildTextureView)
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

alm::fw::RenderStagesWindow::~RenderStagesWindow() = default;

alm::fw::RenderStagesWindow::BufferView::BufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
	const std::string& bufferId, alm::gfx::RGBufferViewTicket ticket)
	: renderStageId(rsId)
	, accessMode(accessMode)
	, bufferId(bufferId)
	, ticket(ticket)
{
	memEditor = std::make_unique<MemoryEditor>();
	memEditor->ReadOnly = true;
}

alm::fw::RenderStagesWindow::BufferView::~BufferView() = default;

alm::fw::RenderStagesWindow::BufferView::BufferView(BufferView&& other) noexcept = default;

alm::fw::RenderStagesWindow::BufferView& alm::fw::RenderStagesWindow::BufferView::operator=(BufferView&& other) noexcept = default;

void alm::fw::RenderStagesWindow::AddTextureView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& textureId)
{
    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_TextureViews)
    {
        if (entry.renderStageId == rsId && entry.accessMode == accessMode && entry.textureId == textureId)
        {
            m_TexViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestTextureView(rsId, accessMode, renderGraph->GetTextureHandle(textureId));
    m_TextureViews.emplace_back(rsId, accessMode, textureId, ticket);
}

void alm::fw::RenderStagesWindow::AddBufferView(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& bufferId)
{
    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_BufferViews)
    {
        if (entry.renderStageId == rsId && entry.accessMode == accessMode && entry.bufferId == bufferId)
        {
            m_BufferViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestBufferView(rsId, accessMode, renderGraph->GetBufferHandle(bufferId));
    m_BufferViews.emplace_back(rsId, accessMode, bufferId, ticket);
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

    // The inspection windows outlive the main window: build them while any is open
    BuildViews();
}

void alm::fw::RenderStagesWindow::BuildViews()
{
    for (auto it = m_TextureViews.begin(); it != m_TextureViews.end();)
    {
        if (m_TexViewFocus == it->ticket)
        {
            ImGui::SetNextWindowFocus();
        }

        if (BuildTextureView(&(*it)))
        {
            it++;
        }
        else
        {
            m_UI.GetUIRenderView()->GetRenderGraph()->ReleaseTextureView(it->ticket);
            it = m_TextureViews.erase(it);
        }
    }

    for (auto it = m_BufferViews.begin(); it != m_BufferViews.end();)
    {
        if (m_BufferViewFocus == it->ticket)
        {
            ImGui::SetNextWindowFocus();
        }

        if (BuildBufferView(&(*it)))
        {
            it++;
        }
        else
        {
            m_UI.GetUIRenderView()->GetRenderGraph()->ReleaseBufferView(it->ticket);
            it = m_BufferViews.erase(it);
        }
    }

    m_TexViewFocus = { nullptr };
    m_BufferViewFocus = { nullptr };
}

bool alm::fw::RenderStagesWindow::BuildTextureView(TextureView* rsTexView)
{
    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
    bool isOpen = true;

    auto renderStage = renderGraph->GetRenderStage(rsTexView->renderStageId);
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

    // ImGui texture flags live in ImGuiRenderStage (this window is not part of that
    // class hierarchy anymore, so they are qualified).
    using ImGuiRS = alm::gfx::ImGuiRenderStage;
    ImGuiRS::ImGuiTexFlags texFlags = ImGuiRS::ImGuiTexFlags_None;
    bool fitPressed = false;

    //
    // Detais child
    //

    ImGui::BeginChild("##details", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    {
        int id = 0;
        const float labelWidth = 80.f;
        const float labelHeight = 120.f;

        FrameworkUI::ShowPropertyInt("Width", labelWidth, texDesc.width, labelHeight, ++id);
        FrameworkUI::ShowPropertyInt("Height", labelWidth, texDesc.height, labelHeight, ++id);
        FrameworkUI::ShowPropertyInt("Depth", labelWidth, texDesc.depth, labelHeight, ++id);
        FrameworkUI::ShowPropertyInt("ArraySize", labelWidth, texDesc.arraySize, labelHeight, ++id);
        FrameworkUI::ShowPropertyInt("MipLevels", labelWidth, texDesc.mipLevels, labelHeight, ++id);
        FrameworkUI::ShowPropertyText("Format", labelWidth, alm::rhi::GetFormatString(texDesc.format), labelHeight, ++id);

        fitPressed = ImGui::Button("Fit");
        ImGui::SameLine();
        FrameworkUI::ShowToggleButton("Alpha", &rsTexView->applyAlpha);
        ImGui::SameLine();
        if (FrameworkUI::ShowToggleButton("R", &rsTexView->redChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (FrameworkUI::ShowToggleButton("G", &rsTexView->greenChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (FrameworkUI::ShowToggleButton("B", &rsTexView->blueChannel))
        {
            rsTexView->alphaChannel = false;
        }
        ImGui::SameLine();
        if (FrameworkUI::ShowToggleButton("A", &rsTexView->alphaChannel))
        {
            rsTexView->redChannel = false;
            rsTexView->greenChannel = false;
            rsTexView->blueChannel = false;
        }

        if (!rsTexView->applyAlpha)
            texFlags |= ImGuiRS::ImGuiTexFlags_IgnoreAlpha;
        if (!rsTexView->redChannel)
            texFlags |= ImGuiRS::ImGuiTexFlags_HideRedChannel;
        if (!rsTexView->greenChannel)
            texFlags |= ImGuiRS::ImGuiTexFlags_HideGreenChannel;
        if (!rsTexView->blueChannel)
            texFlags |= ImGuiRS::ImGuiTexFlags_HideBlueChannel;
        if (rsTexView->alphaChannel)
            texFlags |= ImGuiRS::ImGuiTexFlags_ShowAlphaChannel;
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
            m_UI.ShowUIImage(tex, tex->GetSampledView(), { imageSize.x, imageSize.y }, { 0.f , 0.f }, { 1.f, 1.f }, 0, 0, texFlags);
            ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    rsTexView->firstShow = false;
    return isOpen;
}

bool alm::fw::RenderStagesWindow::BuildBufferView(BufferView* rsBufferView)
{
    bool isOpen = true;

    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
    auto renderStage = renderGraph->GetRenderStage(rsBufferView->renderStageId);
    if (!renderStage)
        return isOpen;

    alm::rhi::BufferHandle buffer = renderGraph->GetBufferView(rsBufferView->ticket);
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
