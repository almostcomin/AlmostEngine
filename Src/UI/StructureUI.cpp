#include "StructureUI.h"
#include <Windows.h>
#include "Gfx/GfxPCH.h"
#include "Gfx/RenderView.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneLights.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderStage.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "RHI/Device.h"
#include "RHI/Buffer.h"
#include <SDL3/SDL.h>
#include <imgui/imgui_internal.h> // For ImGui::GetCurrentWindow()
#include "imgui_memory_editor.h"

namespace
{

struct RSDepResult
{
    bool hovered;
    bool clicked;
};

void TextRightAligned(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    float textWidth = ImGui::CalcTextSize(buffer).x;
    float colWidth = ImGui::GetColumnWidth();
    float cursorX = ImGui::GetCursorPosX();

    ImGui::SetCursorPosX(cursorX + colWidth - textWidth);
    ImGui::TextUnformatted(buffer);
}

void PropertyRowText(const char* label, const char* value)
{
    ImGui::TableNextRow();
    // Label
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextRightAligned(label);
    // Value (fake input)
    ImGui::TableNextColumn();
    ImGui::PushID(value);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##value", (char*)value, strlen(value), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
    ImGui::PopID();
}

void PropertyRowInt(const char* label, int value)
{
    ImGui::TableNextRow();
    // Label
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextRightAligned(label);
    // Value
    ImGui::TableNextColumn();
    ImGui::PushID(label);
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputInt("##value", &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopID();
}

void PropertyRowFloat(const char* label, float value)
{
    ImGui::TableNextRow();
    // Label
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextRightAligned(label);
    // Value
    ImGui::TableNextColumn();
    ImGui::PushID(label);
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputFloat("##value", &value, 0.f, 0.f, "%.2f", ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopID();
}

void PropertyRowFloat3(const char* label, const float3& value)
{
    ImGui::TableNextRow();
    // Label
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextRightAligned(label);
    // Value
    ImGui::TableNextColumn();
    ImGui::PushID(label);
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputFloat3("##value", (float*)&value.x, "%.2f", ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopID();
}

RSDepResult ShowRSDep(const char* label, const char* value, bool selected, int id)
{
    RSDepResult ret;

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

void ShowPropertyText(const char* label, float labelWidth, const char* value, float valueWidth, int id)
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

void ShowPropertyInt(const char* label, float labelWidth, int value, float valueWidth, int id)
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

void TextAlignedRight(float rightPos, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    float textWidth = ImGui::CalcTextSize(buffer).x;
    ImGui::SetCursorPosX(rightPos - textWidth);
    ImGui::TextUnformatted(buffer);
}

const char* GetTextureDimensionText(alm::rhi::TextureDimension dim)
{
    switch (dim)
    {
    case alm::rhi::TextureDimension::Texture1D: return "Texture1D";
    case alm::rhi::TextureDimension::Texture1DArray: return "Texture1DArray";
    case alm::rhi::TextureDimension::Texture2D: return "Texture2D";
    case alm::rhi::TextureDimension::Texture2DArray: return "Texture2DArray";
    case alm::rhi::TextureDimension::TextureCube: return "TextureCube";
    case alm::rhi::TextureDimension::TextureCubeArray: return "TextureCubeArray";
    case alm::rhi::TextureDimension::Texture2DMS: return "Texture2DMS";
    case alm::rhi::TextureDimension::Texture2DMSArray: return "Texture2DMSArray";
    case alm::rhi::TextureDimension::Texture3D: return "Texture3D";
    default: return "Unknown";
    }
}

void BuildTexture(const char* title, const alm::gfx::LoadedTexture& diffuseTex)
{
    ImGui::SeparatorText(title);
    auto desc = diffuseTex.texture->GetDesc();

    ImGui::BeginTable("texId", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 16.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    PropertyRowText("id", diffuseTex.id.c_str());
    ImGui::EndTable();

    ImGui::BeginTable("##texDesc", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
    ImGui::TableSetupColumn("txt0", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("val0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("txt1", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("val1", ImGuiTableColumnFlags_WidthStretch, 1.0f);

    auto propString = [](const char* label, const char* value)
    {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        TextRightAligned(label);
        ImGui::TableNextColumn();
        ImGui::PushID(label);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##value", (char*)value, strlen(value), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        ImGui::PopStyleColor();
        ImGui::PopID();
    };

    auto propInt = [](const char* label, int value)
    {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        TextRightAligned(label);
        ImGui::TableNextColumn();
        ImGui::PushID(label);
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##value", &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        ImGui::PopID();
    };

    // First row
    ImGui::TableNextRow();
    propInt("Width", desc.width);
    propInt("Height", desc.width);
    // Second row
    ImGui::TableNextRow();
    propInt("Depth", desc.depth);
    propInt("ArraySize", desc.arraySize);
    // Third row
    ImGui::TableNextRow();
    propInt("Mip levels", desc.mipLevels);
    propString("Dimension", GetTextureDimensionText(desc.dimension));

    ImGui::EndTable();
}

} // anonymous namespace

StructureUI::StructureUI(alm::weak<alm::gfx::RenderView> renderView, SDL_Window* window, alm::gfx::ShadowmapRenderStage* shadowmapRS, 
                         alm::gfx::ToneMappingRenderStage* tonemappingRS, alm::gfx::DeviceManager* deviceManager) :
    m_Window{ window },
    m_DeviceManager{ deviceManager },
    m_RenderView{ renderView },
    m_ShowSettings{ false },
    m_ShowSceneWindow{ false },
    m_SelectedNode{ nullptr },
    m_ShowResourcesWindow{ false },
    m_ShowRenderStages{ false },
    m_ShadowmapRS{ shadowmapRS },
    m_ShowLuminanceHistogram{ false },
    m_LumHistogramBufferTicket{ nullptr },
    m_TonemappingRS{ tonemappingRS }
{
    if (m_ShadowmapRS)
    {
        m_ShadowmapSize = m_ShadowmapRS->GetSize();
    }
}

StructureUI::~StructureUI()
{}

void StructureUI::BuildUI()
{
    if (!m_Data.ShowUI)
        return;

    ImGuiIO const& io = ImGui::GetIO();

    BeginFullScreenWindow();
    {
        if (!m_RenderView->GetScene())
        {
            DrawCenteredText("Click File->Open to open a scene file");
        }
    }
    EndFullScreenWindow();

    BuildMainMenu();
    BuildBottomBar();

    ImGui::DockSpaceOverViewport(
        0, nullptr, ImGuiDockNodeFlags_NoDockingOverCentralNode | ImGuiDockNodeFlags_PassthruCentralNode);

    if (m_ShowSettings)
        BuildSettingsWindow();

    if (m_ShowSceneWindow)
        BuildSceneWindow(&m_ShowSceneWindow);

    if (m_ShowResourcesWindow)
        BuildResourcesWindow(&m_ShowResourcesWindow);

    if (m_ShowRenderStages)
        BuildRenderStagesWindow();

    BuildRSViews();
    BuildLumnincaHistogram();
}

void StructureUI::OnSceneChanged()
{
    m_SelectedNode = nullptr;
}

void StructureUI::BuildMainMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            BuildMenuFile();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {} // Disabled item
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {}
            if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            if (ImGui::MenuItem("Paste", "CTRL+V")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Settings", NULL, m_ShowSettings))
                m_ShowSettings = !m_ShowSettings;
            if (ImGui::MenuItem("Scene", NULL, m_ShowSceneWindow))
                m_ShowSceneWindow = !m_ShowSceneWindow;
            if (ImGui::MenuItem("Resources", NULL, m_ShowResourcesWindow))
                m_ShowResourcesWindow = !m_ShowResourcesWindow;
            if (ImGui::MenuItem("Render Stages", NULL, m_ShowRenderStages))
                m_ShowRenderStages = !m_ShowRenderStages;

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void StructureUI::BuildBottomBar()
{
    const auto renderStats = m_DeviceManager->GetDevice()->GetStats();

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

        ImGui::Text(" FPS: %1.3f", m_Data.FPS);
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
        ImGui::Text("| GPU: %1.2f ms", m_Data.GPUTime);
        xpos += 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        ImGui::Text("| CPU: %1.2f ms", m_Data.CPUTime);
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void StructureUI::BuildMenuFile()
{
    if (ImGui::MenuItem("Open", "Ctrl+O"))
    {
        std::string filename = OpenFileNativeDialog({}, { { "GlTF", "*.gltf" } });
        if (!filename.empty() && m_RequestLoadFile)
        {
            m_RequestLoadFile(filename.c_str());
        }
    }
    if (ImGui::MenuItem("Close"))
    {
        if (m_RequestClose)
            m_RequestClose();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Quit", "Alt+F4")) 
    {
        if (m_RequestQuit)
            m_RequestQuit();
    }
}

void StructureUI::BuildSettingsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(400, 800), ImGuiCond_Once);

    if (!ImGui::Begin("Settings", &m_ShowSettings, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    if (ImGui::CollapsingHeader("Render modes", ImGuiTreeNodeFlags_None))
    {
        std::vector<std::string> renderModes = m_RenderView->GetRenderGraph()->GetRenderModes();
        int selectedIdx = 0;
        for (int i = 0; i < renderModes.size(); ++i)
        {
            if (renderModes[i] == m_RenderView->GetRenderGraph()->GetCurrentRenderMode())
            {
                selectedIdx = i;
                break;
            }
        }

        if (ImGui::BeginListBox("##RenderModesListBox"))
        {
            for (int n = 0; n < renderModes.size(); n++)
            {
                const bool is_selected = (n == selectedIdx);
                if (ImGui::Selectable(renderModes[n].c_str(), is_selected))
                    selectedIdx = n;
                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        m_Data.RenderMode = renderModes[selectedIdx];
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
    {
        auto camera = m_RenderView->GetCamera();

        // Position
        float3 pos = camera->GetPosition();
        if (ImGui::InputFloat3("Position", (float*)&pos))
        {
            camera->SetPosition(pos);
        }

        // Direction
        float yaw = camera->GetYaw();
        float pitch = camera->GetPitch();
        float roll = camera->GetRoll();

        if (ImGui::SliderAngle("Yaw", &yaw, -180.0f, 180.0f))
            camera->SetYaw(yaw);
        if (ImGui::SliderAngle("Pitch", &pitch, -90.0f, 90))
            camera->SetPitch(pitch);
        if (ImGui::SliderAngle("Roll", &roll, -180, 180.f))
            camera->SetRoll(roll);

        // FOV
        float fov = camera->GetVerticalFOV();
        if (ImGui::SliderAngle("FOV", &fov, 0.f, 180.f))
        {
            camera->SetVerticalFov(fov);
        }

        // 3 elements in a row
        float itemWidth = availWidth / 4;

        // Near plane
        ImGui::SetNextItemWidth(itemWidth);
        float znear = camera->GetZNear();
        if (ImGui::InputFloat("Near", &znear, 0.f, 0.f, "%.3f", 0))
        {
            camera->SetZNear(znear);
        }

        // Speed
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + itemWidth / 2.f);
        float speed = m_Data.CameraSpeed;
        if (ImGui::InputFloat("Speed", &speed, 0.f, 0.f, "%.3f", 0))
        {
            m_Data.CameraSpeed = speed;
        }
    }

    if (ImGui::CollapsingHeader("Debug view", ImGuiTreeNodeFlags_None))
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

        ImGui::Checkbox("Mesh BBoxes", &m_Data.ShowMeshBBoxes);
        ImGui::SameLine(availWidth / 2);
        ImGui::Checkbox("Light BBoxes", &m_Data.ShowLightBBoxes);
    }

    if (ImGui::CollapsingHeader("Shadowmap", ImGuiTreeNodeFlags_None) && m_ShadowmapRS)
    {
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Enabled##Shadowmap", &m_Data.ShadowmapEnabled);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Show##Shadowmap", &m_Data.ShowShadowmap);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(availWidth / 4);
        ImGui::InputInt("Depth Bias##Shadowmap", &m_Data.ShadowmapDepthBias);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 4);
        ImGui::InputFloat("Slope scaled Depth Bias##Shadowmap", &m_Data.ShadowmapSlopeScaledDepthBias);

        ImGui::Spacing();

        float itemWidth = availWidth / 5;
        ImGui::SetCursorPosX(style.ItemSpacing.x);

        ImGui::Text("Size");
        ImGui::SameLine();
        ImGui::SetCursorPosX(itemWidth + style.ItemSpacing.x);

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizex", (int*)&m_ShadowmapSize.x, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizey", (int*)&m_ShadowmapSize.y, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        if (ImGui::Button("Reset", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
        {
            m_ShadowmapSize = m_ShadowmapRS->GetSize();
            m_Data.ShadowmapSize = m_ShadowmapSize;
        }
        ImGui::SameLine();

        if (ImGui::Button("Apply", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
        {
            m_Data.ShadowmapSize = m_ShadowmapSize;
        }
    }

    if (ImGui::CollapsingHeader("Directional light"))
    {
        const float minAzimuth = -180.0;
        const float maxAzimuth = 180.0;
        const float minElevation = -90.0;
        const float maxElevation = 90.0;

        m_Data.SunParamsUpdated |= ImGui::SliderScalar(
            "Azimuth", ImGuiDataType_Float, &m_Data.SunParams.AzimuthDeg, &minAzimuth, &maxAzimuth, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        m_Data.SunParamsUpdated |= ImGui::SliderScalar(
            "Elevation", ImGuiDataType_Float, &m_Data.SunParams.ElevationDeg, &minElevation, &maxElevation, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        m_Data.SunParamsUpdated |= ImGui::ColorEdit3(
            "Color", &m_Data.SunParams.Color.x, ImGuiColorEditFlags_Float);
        m_Data.SunParamsUpdated |= ImGui::SliderFloat(
            "Irradiance", &m_Data.SunParams.Irradiance, 0.f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        m_Data.SunParamsUpdated |= ImGui::SliderFloat(
            "Angular Size", &m_Data.SunParams.AngularSizeDeg, 0.0f, 20.f);
    }

    if (ImGui::CollapsingHeader("Ambient"))
    {
        m_Data.AmbientParamsUpdated |= ImGui::SliderFloat("Intensity", &m_Data.AmbientParams.Intensity, 0.f, 1.f);
        m_Data.AmbientParamsUpdated |= ImGui::ColorEdit3("Sky Color", &m_Data.AmbientParams.SkyColor.x, ImGuiColorEditFlags_Float);
        m_Data.AmbientParamsUpdated |= ImGui::ColorEdit3("Ground Color", &m_Data.AmbientParams.GroundColor.x, ImGuiColorEditFlags_Float);
    }

    if (ImGui::CollapsingHeader("Material channels"))
    {
        if (ImGui::RadioButton("Disabled##matDisabled", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;
        if(ImGui::RadioButton("Base Color##matBaseColor", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor;
        if (ImGui::RadioButton("Metalness##matMetalness", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness;
        if (ImGui::RadioButton("Anisotropy##matAnisotropy", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy;
        if (ImGui::RadioButton("Roughness##matRoughness", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness;
        if (ImGui::RadioButton("Scattering##matScattering", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering;
        if (ImGui::RadioButton("Translucency##matTranslucency", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency;
        if (ImGui::RadioButton("Normal Map##matNormalMap", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap;
        if (ImGui::RadioButton("Occlusion Map##matOcclusionMap", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap;
        if (ImGui::RadioButton("Emissive##matEmissive", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive;
        if (ImGui::RadioButton("Specular F0##matSpecularF0", m_Data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0))
            m_Data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0;
    }

    if (ImGui::CollapsingHeader("SSAO"))
    {
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Enabled##SSAO", &m_Data.SSAOEnabled);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Show##SSAO", &m_Data.ShowSSAO);

        ImGui::InputFloat("Radius##SSAO", &m_Data.SSAO_Radius);    
        ImGui::InputFloat("Power##SSAO", &m_Data.SSAO_Power);
        ImGui::InputFloat("Bias##Bias", &m_Data.SSAO_Bias);
    }

    if (ImGui::CollapsingHeader("Bloom"))
    {
        ImGui::Checkbox("Enabled##Bloom", &m_Data.bloomEnabled);
        ImGui::InputFloat("Radius##Bloom", &m_Data.bloomRadius);
        ImGui::InputFloat("Strength##Bloom", &m_Data.bloomStrength);

        // Calc whats the max mip chain
        alm::rhi::TextureHandle tex = m_RenderView->GetRenderGraph()->GetTexture("SceneColor");
        int maxMips = static_cast<int>(std::floor(std::log2((std::max)(tex->GetDesc().width, tex->GetDesc().height))));
        ImGui::SliderInt("MaxMip##Bloom", &m_Data.bloomMaxMip, 1, maxMips);
    }

    if (ImGui::CollapsingHeader("Tonemapping"))
    {
        ImGui::Checkbox("Enabled##Tonemapping", &m_Data.tonemappingEnabled);

        alm::rhi::ColorSpace colorSpace = m_DeviceManager->GetColorSpace();
        ImGui::SameLine(0.f, availWidth / 8);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        TextRightAligned("Color Space: %s", colorSpace == alm::rhi::ColorSpace::HDR10_ST2084 ? "HDR10" : "SDR");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Luminance

        const float minLogLuminance = -20.f;
        const float maxLogLuminance = 0.f;
        ImGui::SliderScalar("Min log luminance", ImGuiDataType_Float, &m_Data.minLogLuminance, &minLogLuminance, &maxLogLuminance, "%.3f");

        const float minLogRange = 0.f;
        const float maxLogRange = 36.f;
        ImGui::SliderScalar("Log luminance range", ImGuiDataType_Float, &m_Data.logLuminanceRange, &minLogRange, &maxLogRange, "%.3f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const float minLum = exp2(m_Data.minLogLuminance);
        const float maxLum = minLum * std::exp2(m_Data.logLuminanceRange);
        ImGui::Text("Luminance [%1.6f .. %1.6f]", minLum, maxLum);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::SliderFloat("Middle Gray Nits", &m_Data.middleGrayNits, 0.f, 1000.f);
        ImGui::SliderFloat("Paper White Nits", &m_Data.paperWhiteNits, 0.f, 1000.f);

        ImGui::Spacing();

        ImGui::InputFloat("Dark to light adaptation speed", &m_Data.adaptationUpSpeed);
        ImGui::InputFloat("Light to dark adaptation speed", &m_Data.adaptationDownSpeed);

        ImGui::Spacing();

        m_ShowLuminanceHistogram |= ImGui::Button("View Histogram");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float minExposureBias = 0.f;
        const float maxExposureBias = 1.f;
        ImGui::SliderScalar("SDR Exposure Bias", ImGuiDataType_Float, &m_Data.sdrExposureBias, &minExposureBias, &maxExposureBias, "%.3f");
    }

    ImGui::End();
}

void StructureUI::BuildSceneWindow(bool* p_open)
{
    auto scene = m_RenderView->GetScene();
    ImGui::SetNextWindowSize(ImVec2(800, 1000), ImGuiCond_Once);
    if (!ImGui::Begin("Scene view", p_open, ImGuiWindowFlags_None) || !scene || !scene->GetSceneGraph() || !scene->GetSceneGraph()->GetRoot())
    {
        ImGui::End();
        return;
    }
    if (!m_SelectedNode)
        m_SelectedNode = scene->GetSceneGraph()->GetRoot().get();

    if (ImGui::BeginChild("##tree", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        // Left side
        if (ImGui::BeginTable("##bg", 1, ImGuiTableFlags_RowBg))
        {
            uint32_t pushid_count = 0;
            alm::gfx::SceneGraph::Walker walker(*scene->GetSceneGraph());
            while (walker)
            {
                const auto* node = (*walker).get();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID((const void*)node);
                ++pushid_count;
                ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
                tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;// Standard opening mode as we are likely to want to add selection afterwards
                tree_flags |= ImGuiTreeNodeFlags_NavLeftJumpsToParent;  // Left arrow support
                tree_flags |= ImGuiTreeNodeFlags_SpanFullWidth;         // Span full width for easier mouse reach
                tree_flags |= ImGuiTreeNodeFlags_DrawLinesToNodes;      // Always draw hierarchy outlines
                tree_flags |= ImGuiTreeNodeFlags_DefaultOpen;
                if (node == m_SelectedNode)
                    tree_flags |= ImGuiTreeNodeFlags_Selected;
                if (node->GetChildrenCount() == 0)
                    tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool node_open = ImGui::TreeNodeEx("", tree_flags, "%s", node->GetName().c_str());

                // Make an additional PopID here in case the node was no open because it will not be done in the depth 'while'
                if (!node_open)
                    ImGui::PopID();

                if (ImGui::IsItemFocused())
                    m_SelectedNode = node;

                int depth = 0;
                if (node_open)
                    depth = walker.Next();
                else
                    // plus one because TreeNodeEx returned false, was not pushed, therefore we want to avoid pop
                    depth = walker.NextSibling() + 1;
                while (depth < 1)
                {
                    ImGui::TreePop();
                    ImGui::PopID();
                    ++depth;
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    // Right side
    ImGui::SameLine();
    if (ImGui::BeginChild("##details", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        if (auto* node = m_SelectedNode)
        {
            ImGui::Text("%s", node->GetName().c_str());
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Node", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SeparatorText("Local transform");
                {
                    const alm::gfx::Transform& localTransform = node->GetLocalTransform();
                    ImGui::Text("Position");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat3("##localTrans", (float*)&(localTransform.GetTranslation()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    ImGui::Text("Rotation");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat4("##localRot", (float*)&(localTransform.GetRotation()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    ImGui::Text("Scale");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat3("##localScale", (float*)&(localTransform.GetScale()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                }

                ImGui::SeparatorText("World transform");
                {
                    const alm::gfx::Transform worldTransform{ node->GetWorldTransform() };
                    ImGui::Text("Position");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat3("##worldTrans", (float*)&(worldTransform.GetTranslation()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    ImGui::Text("Rotation");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat4("##worldRot", (float*)&(worldTransform.GetRotation()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    ImGui::Text("Scale");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputFloat3("##worldScale", (float*)&(worldTransform.GetScale()), "%.3f", ImGuiInputTextFlags_ReadOnly);
                }

                if (node->HasBounds(alm::gfx::BoundsType::Mesh))
                {
                    ImGui::SeparatorText("Mesh Bounds");
                    {
                        const alm::math::aabox3f& bbox = node->GetWorldBounds(alm::gfx::BoundsType::Mesh);
                        ImGui::Text("Min");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##meshBboxMin", (float*)&(bbox.min), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::Text("Max");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##meshBboxMax", (float*)&(bbox.max), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    }
                }

                if (node->HasBounds(alm::gfx::BoundsType::Light))
                {
                    ImGui::SeparatorText("Light Bounds");
                    {
                        const alm::math::aabox3f& bbox = node->GetWorldBounds(alm::gfx::BoundsType::Light);
                        ImGui::Text("Min");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##lightBboxMin", (float*)&(bbox.min), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::Text("Max");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##lightBboxMax", (float*)&(bbox.max), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    }
                }

                ImGui::SeparatorText("ContentFlags");
                {
                    const alm::gfx::SceneContentFlags flags = node->GetContentFlags();
                    ImGui::BeginTable("##flags", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
                    ImGui::TableSetupColumn("cb0", ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    ImGui::TableSetupColumn("txt0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("cb1", ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    ImGui::TableSetupColumn("txt1", ImGuiTableColumnFlags_WidthStretch, 1.0f);

                    auto flagCell = [](const char* label, bool value)
                    {
                        ImGui::PushID((const void*)label);
                        ImGui::BeginDisabled();
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("##cb", &value);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(label);
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    };

                    // First row
                    ImGui::TableNextRow();
                    flagCell("OpaqueMeshes", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::OpaqueMeshes));
                    flagCell("DirLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::DirectionalLights));

                    // Second row
                    ImGui::TableNextRow();
                    flagCell("AlphaTestedMeshes", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::AlphaTestedMeshes));
                    flagCell("PointLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::PointLights));

                    // Third row
                    ImGui::TableNextRow();
                    flagCell("BlendedMeshes", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::BlendedMeshes));
                    flagCell("SpotLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::SpotLights));

                    // Four row
                    flagCell("Cameras", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::Cameras));
                    flagCell("Animations", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::Animations));

                    ImGui::EndTable();
                }
            }

            const auto* leaf = node->GetLeaf() ? node->GetLeaf().get() : nullptr;
            if (leaf)
            {
                switch (leaf->GetType())
                {
                case alm::gfx::SceneGraphLeaf::Type::MeshInstance:
                    BuildMeshInstanceLeaf(alm::checked_cast<const alm::gfx::MeshInstance*>(leaf));
                    break;
                case alm::gfx::SceneGraphLeaf::Type::Camera:
                    assert(0);
                    break;
                case alm::gfx::SceneGraphLeaf::Type::DirectionalLight:
                    BuildDirLightLeaf(alm::checked_cast<const alm::gfx::SceneDirectionalLight*>(leaf));
                    break;
                case alm::gfx::SceneGraphLeaf::Type::PointLight:
                    BuildPointLightLeaf(alm::checked_cast<const alm::gfx::ScenePointLight*>(leaf));
                    break;
                case alm::gfx::SceneGraphLeaf::Type::SpotLight:
                    BuildSpotLightLeaf(alm::checked_cast<const alm::gfx::SceneSpotLight*>(leaf));
                    break;
                default:
                    assert(0);
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void StructureUI::BuildResourcesWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Resources view", p_open, ImGuiWindowFlags_None))
    {
        alm::rhi::Device* device = m_DeviceManager->GetDevice();


        ImGui::End();
        return;
    }

    ImGui::End();
}

void StructureUI::BuildRenderStagesWindow()
{
    alm::gfx::RenderGraph* renderGraph = m_RenderView->GetRenderGraph().get();
    const float parentHeight = ImGui::GetIO().DisplaySize.y;
    const float windowHeight = parentHeight * 0.9f;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    ImGui::SetNextWindowSize(ImVec2(320, windowHeight), ImGuiCond_Once);
    std::string title = m_RenderView->GetName() + " - " + renderGraph->GetCurrentRenderMode() + "###RenderViewWindow";
    if (!ImGui::Begin(title.c_str(), &m_ShowRenderStages, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    int propid = 0;
    std::string newHoveredId;
    for (int rs_idx = 0; rs_idx < renderGraph->GetNumRenderStages(); ++rs_idx)
    {
        const auto* rs = renderGraph->GetRenderStage(rs_idx);
        if (ImGui::CollapsingHeader(rs->renderStage->GetDebugName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto addDep = [this, &newHoveredId, &propid, rs]<typename T>(const std::vector<T>& deps, bool isWrite, bool isBuffer)
            {
                for (const auto& dep : deps)
                {
                    const std::string& id = m_RenderView->GetRenderGraph()->GetId(dep.handle);
                    bool selected = (m_RenderStageIOHoveredId == id);

                    RSDepResult result = ShowRSDep(isBuffer ? "BUFFER" : "TEXTURE", id.c_str(), selected, propid++);
                    if (result.hovered)
                    {
                        newHoveredId = id;
                    }
                    if (result.clicked)
                    {
                        if (isBuffer)
                        {
                            AddRenderStageBufferView(
                                rs->renderStage.get(),
                                isWrite ? alm::gfx::RenderGraph::AccessMode::Write : alm::gfx::RenderGraph::AccessMode::Read,
                                id);
                        }
                        else
                        {
                            AddRenderStageTextureView(
                                rs->renderStage.get(),
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
                // find a resolved timer query

                float gpuTimeMs = 0.f;
                int valid = 0;
                for (int i = 0; i < rs->timerQueries.size(); ++i)
                {
                    if (rs->timerQueries[i]->Poll())
                    {
                        gpuTimeMs += rs->timerQueries[i]->GetQueryTimeMs();
                        ++valid;
                    }
                }

                if (valid > 0)
                {
                    gpuTimeMs /= valid;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);

                float cpuTime = std::accumulate(rs->cpuElapsed.begin(), rs->cpuElapsed.end(), 0.0f) / rs->cpuElapsed.size();
                

                ImGui::Text("CPU time: %1.3fms", cpuTime);
                ImGui::SameLine();
                TextRightAligned("GPU time: %1.3fms", gpuTimeMs);
                ImGui::PopStyleColor();
            }
        }
    }
    m_RenderStageIOHoveredId = newHoveredId;

    ImGui::End();
}

void StructureUI::BuildRSViews()
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
            m_RenderView->GetRenderGraph()->ReleaseTextureView(it->ticket);
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
            m_RenderView->GetRenderGraph()->ReleaseBufferView(it->ticket);
            it = m_RSBufferViews.erase(it);
        }
    }

    m_RSTexViewFocus = { nullptr };
    m_RSBufferViewFocus = { nullptr };
}

void StructureUI::BuildLumnincaHistogram()
{
    if (!m_ShowLuminanceHistogram)
        return;

    alm::gfx::RenderGraph* renderGraph = m_RenderView->GetRenderGraph().get();

    if (!m_LumHistogramBufferTicket.IsValid())
    {
        m_LumHistogramBufferTicket = renderGraph->RequestBufferView(
            m_TonemappingRS, alm::gfx::RenderGraph::AccessMode::Write, renderGraph->GetBufferHandle("LuminanceHistogram"));
    }
    if (!m_LumHistogramBufferTicket.IsValid())
    {
        LOG_ERROR("Failed to requesting histogram buffer ticket");
        return;
    }

    ImGui::SetNextWindowSize(ImVec2{ 600, 200 }, ImGuiCond_Once);
    if (!ImGui::Begin("Luminance histogram", &m_ShowLuminanceHistogram, ImGuiWindowFlags_None))
    {
        ImGui::End();
        if (!m_ShowLuminanceHistogram)
        {
            renderGraph->ReleaseBufferView(m_LumHistogramBufferTicket);
            m_LumHistogramBufferTicket = { nullptr };
        }
        return;
    }

    alm::gfx::ToneMappingRenderStage::Stats stats = m_TonemappingRS->GetStats();

    alm::rhi::TextureHandle toneMappedTex = renderGraph->GetTexture("ToneMapped");
    assert(toneMappedTex);

    alm::rhi::BufferHandle buffer = renderGraph->GetBufferView(m_LumHistogramBufferTicket);
    if (buffer)
    {
        void* data_ptr = buffer->Map();
        auto getValueLog2 = [](void* data, int idx) -> float
        {
            uint32_t* arr = (uint32_t*)data;
            return log2f((float)arr[idx] + 1.0f);
        };
        auto getValueLinear = [](void* data, int idx) -> float
            {
                uint32_t* arr = (uint32_t*)data;
                return (float)arr[idx];
            };

        ImVec2 size = ImGui::GetContentRegionAvail();
        size.y -= ImGui::GetFrameHeightWithSpacing();
        if (m_LumHistogramMode == 0)
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLog2, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, log2f(stats.totalPixels), size);
        }
        else
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLinear, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, stats.totalPixels, size);
        }

        buffer->Unmap();

        if (ImGui::RadioButton("Log2", m_LumHistogramMode == 0)) 
            m_LumHistogramMode = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Linear", m_LumHistogramMode == 1))
            m_LumHistogramMode = 1;
        ImGui::SameLine();
        ImGui::Text("| Min: %1.3f | Max: %1.3f | Avg: %1.3f | Bin: %1.1f", stats.minLuminance, stats.maxLuminance, stats.avgLuminance, stats.avgBin);
    }

    ImGui::End();
}

bool StructureUI::BuildRSTexView(RenderStageTextureView* rsTexView)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderView->GetRenderGraph().get();
    bool isOpen = true;

    alm::rhi::TextureHandle tex = renderGraph->GetTextureView(rsTexView->ticket);
    if (!tex)
        return isOpen;

    std::stringstream title;
    title << rsTexView->renderStage->GetDebugName() << " - ";
    if (rsTexView->accessMode == alm::gfx::RenderGraph::AccessMode::Read)
        title << "Read";
    else
        title << "Write";
    title << " - " << rsTexView->id.c_str();

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
            ShowImage(tex, { imageSize.x, imageSize.y }, { 0.f , 0.f }, { 1.f, 1.f }, texFlags);
            ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    rsTexView->firstShow = false;
    return isOpen;
}

bool StructureUI::BuildRSBufferView(RenderStageBufferView* rsBufferView)
{
    bool isOpen = true;
    alm::rhi::BufferHandle buffer = m_RenderView->GetRenderGraph()->GetBufferView(rsBufferView->ticket);
    if (!buffer)
        return isOpen;

    std::stringstream title;
    title << rsBufferView->renderStage->GetDebugName() << " - ";
    if (rsBufferView->accessMode == alm::gfx::RenderGraph::AccessMode::Read)
        title << "Read";
    else
        title << "Write";
    title << " - " << rsBufferView->id.c_str();
    std::string titleStr = title.str();

    void* dataPtr = buffer->Map();
    rsBufferView->memEditor->DrawWindow(titleStr.c_str(), dataPtr, buffer->GetDesc().sizeBytes);
    buffer->Unmap();

    return rsBufferView->memEditor->Open;
}

void StructureUI::BuildMeshInstanceLeaf(const alm::gfx::MeshInstance* leaf)
{
    const auto& mesh = leaf->GetMesh();

    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginTable("MatProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        PropertyRowText("Name", mesh->GetName().c_str());
        ImGui::EndTable();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const alm::rhi::PrimitiveTopology topo = mesh->GetPrimitiveTopology();

        ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const char* topoStr = nullptr;
        switch (topo)
        {
        case alm::rhi::PrimitiveTopology::PointList:
            PropertyRowText("Primitive type", "PointList");
            break;
        case alm::rhi::PrimitiveTopology::TriangleList:
            PropertyRowText("Primitive type", "TriangleList");
            break;
        default:
            assert(0);
        }
    
        int primCount = alm::rhi::GetPrimitiveCount(mesh->GetIndexCount(), topo);
        PropertyRowInt("Primitive count", primCount);

        PropertyRowInt("Index count", mesh->GetIndexCount());

        int vertexCount = mesh->GetVertexBuffer()->GetDesc().sizeBytes / mesh->GetVertexFormat().VertexStride;
        PropertyRowInt("Vertex count", vertexCount);

        PropertyRowInt("IB size bytes", mesh->GetIndexBuffer()->GetDesc().sizeBytes);
        PropertyRowInt("VB size bytes", mesh->GetVertexBuffer()->GetDesc().sizeBytes);

        ImGui::EndTable();

        ImGui::SeparatorText("Vertex format");
        {
            const alm::gfx::Mesh::VertexFormat& vertexFormat = mesh->GetVertexFormat();
            const float cb_w = ImGui::GetFrameHeight();
            ImGui::BeginTable("##flags", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
            ImGui::TableSetupColumn("cb0", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("txt0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("cb1", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("txt1", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            auto flagCell = [](const char* label, bool value)
            {
                ImGui::PushID((const void*)label);
                ImGui::BeginDisabled();
                ImGui::TableNextColumn();
                ImGui::Checkbox("##cb", &value);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::EndDisabled();
                ImGui::PopID();
            };

            // First row
            ImGui::TableNextRow();
            flagCell("Position", vertexFormat.PositionOffset != UINT32_MAX);
            flagCell("TexCoord0", vertexFormat.TexCoord0Offset != UINT32_MAX);

            // Second row
            ImGui::TableNextRow();
            flagCell("Normal", vertexFormat.NormalOffset != UINT32_MAX);
            flagCell("TexCoord1", vertexFormat.TexCoord1Offset != UINT32_MAX);

            // Third row
            ImGui::TableNextRow();
            flagCell("Tangent", vertexFormat.TangentOffset != UINT32_MAX);
            flagCell("Color", vertexFormat.ColorOffset != UINT32_MAX);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& mat = mesh->GetMaterial();
        if (mat)
        {
            ImGui::BeginTable("MatProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            PropertyRowText("Name", mat->GetName().c_str());
            ImGui::EndTable();

            if(auto tex = mat->GetBaseColorTexture())
                BuildTexture("Diffuse texture", *tex);
            if (auto tex = mat->GetMetalRoughTexture())
                BuildTexture("Metal-rough texture", *tex);
            if (auto tex = mat->GetNormalTexture())
                BuildTexture("Normal texture", *tex);
            if (auto tex = mat->GetEmissiveTexture())
                BuildTexture("Emissive texture", *tex);
            if (auto tex = mat->GetOcclusionTexture())
                BuildTexture("Occlusion texture", *tex);
        }
    }
}

void StructureUI::BuildDirLightLeaf(const alm::gfx::SceneDirectionalLight* light)
{
    if (ImGui::CollapsingHeader("DirectionalLight", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginTable("DirectionalLightProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        PropertyRowText("Name", light->GetName().c_str());
        ImGui::EndTable();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        // Color
        {
            ImGui::TableNextRow();
            // Label
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            TextRightAligned("Color");
            // Value
            ImGui::TableNextColumn();
            ImGui::PushID("LightColor");
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::ColorEdit3("", (float*)&(light->GetColor().x), ImGuiColorEditFlags_Float);
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        PropertyRowFloat3("Direction", light->GetDirection());
        PropertyRowFloat("Irradiance", light->GetIrradiance());
        PropertyRowFloat("AngularSize", light->GetAngularSize());

        ImGui::EndTable();
    }
}

void StructureUI::BuildPointLightLeaf(const alm::gfx::ScenePointLight* light)
{
    if (ImGui::CollapsingHeader("PointLight", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginTable("PointLightProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        PropertyRowText("Name", light->GetName().c_str());
        ImGui::EndTable();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        // Color
        {
            ImGui::TableNextRow();
            // Label
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            TextRightAligned("Color");
            // Value
            ImGui::TableNextColumn();
            ImGui::PushID("LightColor");
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::ColorEdit3("", (float*)&(light->GetColor().x), ImGuiColorEditFlags_Float);
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        PropertyRowFloat("Intensity", light->GetIntensity());
        PropertyRowFloat("Range", light->GetRange());
        PropertyRowFloat("Radius", light->GetRadius());

        ImGui::EndTable();
    }
}

void StructureUI::BuildSpotLightLeaf(const alm::gfx::SceneSpotLight* light)
{
    if (ImGui::CollapsingHeader("SpotLight", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginTable("SpotLightProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        PropertyRowText("Name", light->GetName().c_str());
        ImGui::EndTable();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        // Color
        {
            ImGui::TableNextRow();
            // Label
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            TextRightAligned("Color");
            // Value
            ImGui::TableNextColumn();
            ImGui::PushID("LightColor");
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::ColorEdit3("", (float*)&(light->GetColor().x), ImGuiColorEditFlags_Float);
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        PropertyRowFloat3("Direction", light->GetDirection());
        PropertyRowFloat("Intensity", light->GetIntensity());
        PropertyRowFloat("Range", light->GetRange());
        PropertyRowFloat("Radius", light->GetRadius());
        PropertyRowFloat("InnerConeAngle", light->GetInnerConeAngle());
        PropertyRowFloat("OuterrConeAngle", light->GetOuterConeAngle());


        ImGui::EndTable();
    }
}

void StructureUI::AddRenderStageTextureView(alm::gfx::RenderStage* renderStage, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& id)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderView->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_RSTextureViews)
    {
        if (entry.renderStage == renderStage && entry.accessMode == accessMode && entry.id == id)
        {
            m_RSTexViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestTextureView(renderStage, accessMode, renderGraph->GetTextureHandle(id));
    m_RSTextureViews.emplace_back(renderStage, accessMode, id, ticket);
}

void StructureUI::AddRenderStageBufferView(alm::gfx::RenderStage* renderStage, alm::gfx::RenderGraph::AccessMode accessMode, const std::string& id)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderView->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_RSBufferViews)
    {
        if (entry.renderStage == renderStage && entry.accessMode == accessMode && entry.id == id)
        {
            m_RSBufferViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestBufferView(renderStage, accessMode, renderGraph->GetBufferHandle(id));
    MemoryEditor* memEditor = new MemoryEditor;
    memEditor->ReadOnly = true;
    m_RSBufferViews.emplace_back(renderStage, accessMode, id, ticket, std::unique_ptr<MemoryEditor>{ memEditor });
}

std::string StructureUI::OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters)
{
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(m_Window);
    HWND hWnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[260] = { 0 };       // if using TCHAR macros
    if (!filename.empty())
    {
        strcpy_s<260>(szFile, filename.c_str());
    }

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    if (!filters.empty())
        ofn.lpstrDefExt = filters[0].second.c_str();

    std::vector<char> fil;
    if (!filters.empty())
    {
        for (const auto p : filters)
        {
            fil.insert(fil.end(), p.first.begin(), p.first.end());
            fil.push_back('\0');
            fil.insert(fil.end(), p.second.begin(), p.second.end());
            fil.push_back('\0');
        }
        const char* szAll = "All";
        fil.insert(fil.end(), szAll, szAll + strlen(szAll) + 1);
        const char* szAllFilter = "*.*";
        fil.insert(fil.end(), szAllFilter, szAllFilter + strlen(szAllFilter) + 1);
        fil.push_back('\0');
    }
    ofn.lpstrFilter = fil.data();

    ofn.nFilterIndex = 0;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        return ofn.lpstrFile;
    }
    else
    {
        return {};
    }
}

std::string StructureUI::SaveFileNativeDialog(const std::string& filename)
{
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(m_Window);
    HWND hWnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[260] = { 0 };       // if using TCHAR macros
    if (!filename.empty())
    {
        strcpy_s<260>(szFile, filename.c_str());
    }

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrDefExt = "rms";
    ofn.lpstrFilter = "rms\0*.rms\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn))
    {
        return ofn.lpstrFile;
    }
    else
    {
        return {};
    }
}