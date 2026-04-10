#include "Framework/FrameworkPCH.h"
#include "Framework/FrameworkUI.h"
#include "gfx/DeviceManager.h"
#include "rhi/Device.h"
#include <imgui/imgui_internal.h> // For ImGui::GetCurrentWindow()

namespace
{
} // anonymous namespace

void alm::fw::FrameworkUI::BuildUI()
{
    if (m_ShowBottomBar)
    {
        BuildBottomBar();
    }

    BuildTextureWindows();
}

void alm::fw::FrameworkUI::AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture)
{
    m_TextureWindows.push_back(UITextureWindow{
        .title = title,
        .texture = texture });
}

void alm::fw::FrameworkUI::SetRenderStats(float fps, float cpuTime, float gpuTime)
{
    m_FPS = fps;
    m_CPUTime = cpuTime;
    m_GPUTime = gpuTime;
}

bool alm::fw::FrameworkUI::ShowToggleButton(const char* label, bool* v)
{
    bool pressed = false;
    bool newV = *v;
    if (*v)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(label))
    {
        newV = !newV;
        pressed = true;
    }
    if (*v)
    {
        ImGui::PopStyleColor();
    }
    *v = newV;
    return pressed;
}

void alm::fw::FrameworkUI::ShowPropertyInt(const char* label, float labelWidth, int value, float valueWidth, int id)
{
    ImGui::PushID(id);
    float startX = ImGui::GetCursorPosX();

    // Label
    ImGui::SetNextItemWidth(labelWidth);
    ImGui::TextUnformatted(label);

    ImGui::SameLine(startX + labelWidth + 8.0f);

    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(valueWidth);
    ImGui::InputInt("##value", &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();

    ImGui::PopID();
}

void alm::fw::FrameworkUI::ShowPropertyText(const char* label, float labelWidth, const char* value, float valueWidth, int id)
{
    ImGui::PushID(id);
    float startX = ImGui::GetCursorPosX();

    // Label
    ImGui::SetNextItemWidth(labelWidth);
    ImGui::TextUnformatted(label);

    ImGui::SameLine(startX + labelWidth + 8.0f);

    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(valueWidth);
    ImGui::InputText("##value", (char*)value, strlen(value), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();

    ImGui::PopID();
}

void alm::fw::FrameworkUI::BuildBottomBar()
{
    auto* deviceManager = GetDeviceManager();
    const auto renderStats = deviceManager->GetDevice()->GetStats();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float statusBarHeight = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2{ vp->Pos.x, vp->Pos.y + vp->Size.y - statusBarHeight });
    ImGui::SetNextWindowSize(ImVec2{ vp->Size.x, statusBarHeight });

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##StatusBar", nullptr, flags);
    {
        float textHeight = ImGui::GetTextLineHeight();
        float paddingY = ImGui::GetStyle().WindowPadding.y;

        // Centro vertical real
        float cursorY = (statusBarHeight - textHeight) * 0.5f;

        ImGui::SetCursorPosY(cursorY);

        ImGui::Text(" FPS: %1.3f", m_FPS);
        float xpos = 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        ImGui::Text("| Draw calls: %d", renderStats.DrawCalls);
        xpos += 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        ImGui::Text("| Primitives: %d", renderStats.PrimitiveCount);
        xpos += 200.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        ImGui::Text("| GPU: %1.2f ms", m_GPUTime);
        xpos += 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        ImGui::Text("| CPU: %1.2f ms", m_CPUTime);
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void alm::fw::FrameworkUI::BuildTextureWindows()
{
    auto it = m_TextureWindows.begin();
    while (it != m_TextureWindows.end())
    {
        if (!BuildTextureWindow(*it))
        {
            it = m_TextureWindows.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool alm::fw::FrameworkUI::BuildTextureWindow(UITextureWindow& tw)
{
    bool isOpen = true;
    const auto& texDesc = tw.texture->GetDesc();
    ImVec2 imageSize{ tw.defaultImageWidth, tw.defaultImageWidth * texDesc.height / texDesc.width };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float detailsHeight = ImGui::GetTextLineHeightWithSpacing() * 8.0f + ImGui::GetFrameHeightWithSpacing();
    const float childBorder = style.ChildBorderSize * 2.0f;
    const float spacingY = style.ItemSpacing.y;
    const float scrollbar = style.ScrollbarSize;
    const float windowWidth = imageSize.x + style.WindowPadding.x * 2.0f + style.WindowBorderSize * 2.0f + childBorder * 2 + 1.0f + 40.f;
    const float windowHeight = detailsHeight + spacingY + imageSize.y + style.WindowPadding.y * 2.0f + style.WindowBorderSize *
        2.0f + childBorder + scrollbar + 1.0f + 40.f;

    ImGui::SetNextWindowSize(ImVec2{ windowWidth, windowHeight }, ImGuiCond_Appearing);

    if (!ImGui::Begin(tw.title.c_str(), &isOpen, ImGuiWindowFlags_None))
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
        ShowToggleButton("Opaque", &tw.opaque);
        
        ImGui::SameLine(0.f, 20.f);

        bool showRed = tw.redChannel && !tw.alphaChannel;
        if (ShowToggleButton("R", &showRed))
        {
            if (tw.alphaChannel)
            {
                tw.redChannel = true;
                tw.greenChannel = false;
                tw.blueChannel = false;
                tw.alphaChannel = false;
            }
            else
            {
                tw.redChannel = showRed;
            }            
        }

        ImGui::SameLine();
        bool showGreen = tw.greenChannel && !tw.alphaChannel;
        if (ShowToggleButton("G", &showGreen))
        {
            if (tw.alphaChannel)
            {
                tw.redChannel = false;
                tw.greenChannel = true;
                tw.blueChannel = false;
                tw.alphaChannel = false;
            }
            else
            {
                tw.greenChannel = showGreen;
            }
        }

        ImGui::SameLine();
        bool showBlue = tw.blueChannel && !tw.alphaChannel;
        if (ShowToggleButton("B", &showBlue))
        {
            if (tw.alphaChannel)
            {
                tw.redChannel = false;
                tw.greenChannel = false;
                tw.blueChannel = false;
                tw.alphaChannel = true;
            }
            else
            {
                tw.blueChannel = showBlue;
            }
        }
        ImGui::SameLine();
        if (ShowToggleButton("A", &tw.alphaChannel))
        {
            tw.alphaChannel = tw.alphaChannel;
        }

        if (!tw.opaque)
            texFlags |= ImGuiTexFlags_IgnoreAlpha;
        if (!showRed)
            texFlags |= ImGuiTexFlags_HideRedChannel;
        if (!showGreen)
            texFlags |= ImGuiTexFlags_HideGreenChannel;
        if (!showBlue)
            texFlags |= ImGuiTexFlags_HideBlueChannel;
        if (tw.alphaChannel)
            texFlags |= ImGuiTexFlags_ShowAlphaChannel;

        std::vector<std::string> mips;
        mips.resize(texDesc.mipLevels);
        for (int i = 0; i < texDesc.mipLevels; ++i)
        {
            mips[i] = std::to_string(i);
        }
        ImGui::SetNextItemWidth(60);
        if (ImGui::BeginCombo("Mip", mips[tw.selectedMip].c_str()))
        {
            for (int i = 0; i < texDesc.mipLevels; ++i)
            {
                if (ImGui::Selectable(mips[i].c_str(), tw.selectedMip == i))
                {
                    tw.selectedMip = i;
                }
            }
            ImGui::EndCombo();
        }

        if (texDesc.dimension == rhi::TextureDimension::Texture3D)
        {
            ImGui::SameLine(0.f, 60.0f);

            ImGui::SetNextItemWidth(60);
            std::vector<std::string> depth;
            depth.resize(texDesc.depth);
            for (int i = 0; i < texDesc.depth; ++i)
            {
                depth[i] = std::to_string(i);
            }
            if (ImGui::BeginCombo("Depth", depth[tw.selectedSlice].c_str()))
            {
                for (int i = 0; i < texDesc.depth; ++i)
                {
                    if (ImGui::Selectable(depth[i].c_str(), tw.selectedSlice == i))
                    {
                        tw.selectedSlice = i;
                    }
                }
                ImGui::EndCombo();
            }
        }

    }
    ImGui::EndChild();

    //
    // Image child
    //

    ImGui::SetNextWindowContentSize(imageSize);
    ImGuiWindowFlags imageChildFlags = ImGuiWindowFlags_None;
    if (tw.firstShow)
    {
        imageChildFlags |= ImGuiWindowFlags_NoScrollbar;
    }
    else
    {
        imageChildFlags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    ImVec2 childSize{ 0.f, 0.f };
    if (tw.firstShow)
    {
        childSize = { imageSize.x, imageSize.y };
    }

    ImGui::BeginChild("##image", childSize, ImGuiChildFlags_None, imageChildFlags);
    {
        ImRect clip = ImGui::GetCurrentWindow()->ClipRect;
        ImVec2 avail = clip.GetSize();//ImGui::GetContentRegionAvail();

        if (tw.firstShow)
        {
            float scale = (std::max)((float)texDesc.width / avail.x, (float)texDesc.height / avail.y);
            tw.defaultImageWidth = texDesc.width / scale - 1.f;
        }
        else
        {
            if (fitPressed)
            {
                float scale = (std::max)((float)texDesc.width / avail.x, (float)texDesc.height / avail.y);
                tw.defaultImageWidth = texDesc.width / scale - 1.f;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ShowImage(tw.texture, { imageSize.x, imageSize.y }, { 0.f , 0.f }, { 1.f, 1.f }, tw.selectedMip, tw.selectedSlice, texFlags);
            ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    tw.firstShow = false;
    return isOpen;
}