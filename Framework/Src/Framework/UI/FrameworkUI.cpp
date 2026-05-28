#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/CameraController.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/SceneLights.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "rhi/Device.h"
#include <imgui/imgui_internal.h> // For ImGui::GetCurrentWindow()
#include <SDL3/SDL.h>
#include <commdlg.h>
#include "imgui_memory_editor.h"

namespace
{
    struct RSDepActionResult
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

    void BuildMeshInstanceLeaf(const alm::gfx::MeshInstance* leaf)
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

                if (auto tex = mat->GetBaseColorTexture())
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

    void BuildDirLightLeaf(const alm::gfx::SceneDirectionalLight* light)
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

    void BuildPointLightLeaf(const alm::gfx::ScenePointLight* light)
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

    void BuildSpotLightLeaf(const alm::gfx::SceneSpotLight* light)
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

alm::fw::FrameworkUI::FrameworkUI() : m_Window(nullptr)
{}

alm::fw::FrameworkUI::~FrameworkUI()
{}

void alm::fw::FrameworkUI::Init(SDL_Window* window, weak<gfx::Scene> scene, weak<gfx::RenderView> renderView, CameraController* cameraController)
{
    m_Window = window;
    m_Scene = scene;
    m_RenderViewUI = renderView;
    m_CameraController = cameraController;
}

void alm::fw::FrameworkUI::BuildUI()
{
    ImGui::DockSpaceOverViewport(
        0, nullptr, ImGuiDockNodeFlags_NoDockingOverCentralNode | ImGuiDockNodeFlags_PassthruCentralNode);

    BuildMainMenu();
    BuildBottomBar();

    BuildSettingsWindow();
    BuildSceneGraphWindow();
    BuildRenderStagesWindow();

    BuildLumninanceHistogram();
    BuildTextureWindows();
    BuildRSViews();
}

void alm::fw::FrameworkUI::AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture)
{
    m_TextureWindows.push_back(UITextureWindow{
        .title = title,
        .texture = texture });
}

void alm::fw::FrameworkUI::AddRenderStageTextureWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& textureId)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_RSTextureViews)
    {
        if (entry.renderStageId == rsId && entry.accessMode == accessMode && entry.textureId == textureId)
        {
            m_RSTexViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestTextureView(rsId, accessMode, renderGraph->GetTextureHandle(textureId));
    m_RSTextureViews.emplace_back(rsId, accessMode, textureId, ticket);
}

void alm::fw::FrameworkUI::AddRenderStageBufferWindow(alm::gfx::RenderStageTypeID rsId, alm::gfx::RenderGraph::AccessMode accessMode,
    const std::string& bufferId)
{
    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    // Check if it already exists
    for (auto& entry : m_RSBufferViews)
    {
        if (entry.renderStageId == rsId && entry.accessMode == accessMode && entry.bufferId == bufferId)
        {
            m_RSBufferViewFocus = entry.ticket;
            return;
        }
    }

    auto ticket = renderGraph->RequestBufferView(rsId, accessMode, renderGraph->GetBufferHandle(bufferId));
    MemoryEditor* memEditor = new MemoryEditor;
    memEditor->ReadOnly = true;
    m_RSBufferViews.emplace_back(rsId, accessMode, bufferId, ticket, std::unique_ptr<MemoryEditor>{ memEditor });
}

void alm::fw::FrameworkUI::RegisterMainMenuItem(const std::string& name, std::function<void()> item)
{
    m_MainMenuAdditionalItems.emplace_back(name, item);
}

void alm::fw::FrameworkUI::SetRenderStats(float fps, float cpuTime, float gpuTime)
{
    m_FPS = fps;
    m_CPUTime = cpuTime;
    m_GPUTime = gpuTime;
}

void alm::fw::FrameworkUI::AddBottomBarText(const std::string& text)
{
    m_BottomBarRightAlignTexts.push_back(text);
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

void alm::fw::FrameworkUI::TextRightAlignedPosX(float xpos, const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    float textWidth = ImGui::CalcTextSize(buffer).x;
    xpos -= textWidth;
    ImGui::SetCursorPosX(xpos);
    ImGui::TextUnformatted(buffer);
}

void alm::fw::FrameworkUI::BuildMainMenu()
{
    if (!m_ShowMainMenu)
        return;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
                std::string filename = OpenFileNativeDialog({}, { { "GlTF", "*.gltf" } });
                if (!filename.empty() && m_RequestLoadFile)
                {
                    m_RequestLoadFile(filename.c_str());
                }
            }
            if (ImGui::MenuItem("Merge"))
            {
                std::string filename = OpenFileNativeDialog({}, { { "GlTF", "*.gltf" } });
                if (!filename.empty() && m_RequestMergeFile)
                {
                    m_RequestMergeFile(filename.c_str());
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

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Settings", NULL, m_ShowSettings))
                m_ShowSettings = !m_ShowSettings;
            if (ImGui::MenuItem("Scene Graph", NULL, m_ShowSceneGraphWindow))
                m_ShowSceneGraphWindow = !m_ShowSceneGraphWindow;
            if (ImGui::MenuItem("Render Stages", NULL, m_ShowRenderStages))
                m_ShowRenderStages = !m_ShowRenderStages;

            ImGui::EndMenu();
        }

        for (const auto& item : m_MainMenuAdditionalItems)
        {
            if (ImGui::BeginMenu(item.first.c_str()))
            {
                item.second();
                ImGui::EndMenu();
            }
        }

        ImGui::EndMainMenuBar();
    }
}

void alm::fw::FrameworkUI::BuildBottomBar()
{
    if (!m_ShowBottomBar)
        return;

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

        float cursorX = 0.f;
        const float cursorY = (statusBarHeight - textHeight) * 0.5f;

        ImGui::SetCursorPosX(cursorX);
        ImGui::SetCursorPosY(cursorY);

        ImGui::Text(" FPS: %1.3f", m_FPS);
        cursorX = 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| Draw calls: %d", renderStats.DrawCalls);
        cursorX += 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| Primitives: %d", renderStats.PrimitiveCount);
        cursorX += 200.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| GPU: %1.2f ms", m_GPUTime);
        cursorX += 160.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| CPU: %1.2f ms", m_CPUTime);

        // Right aligned user texts
        cursorX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        for (const std::string& text : m_BottomBarRightAlignTexts)
        {
            std::string actualText = std::format(" | {}", text);
            float textWidth = ImGui::CalcTextSize(actualText.c_str()).x;
            cursorX -= textWidth;
            ImGui::SameLine();
            ImGui::SetCursorPosX(cursorX);
            ImGui::Text(actualText.c_str());
        }
        m_BottomBarRightAlignTexts.clear();
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void alm::fw::FrameworkUI::BuildSettingsWindow()
{
    if (!m_ShowSettings)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Settings", &m_ShowSettings, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    BuildRenderModesSettings();
    BuildCameraSettings(availWidth);
    BuildDebugViewSettings(availWidth);
    BuildShadowmapSettings(availWidth);
    BuildDirectionalLightSettings(availWidth);
    BuildAmbientLightSettings(availWidth);
    BuildSkySettings(availWidth);
    BuildsCloudsSettings();
    BuildMaterialChannelsSettings(availWidth);
    BuildSSAOSettings(availWidth);
    BuildBloomSettings(availWidth);
    BuildTonemappingSettings(availWidth);

    ImGui::End();
}

void alm::fw::FrameworkUI::BuildSceneGraphWindow()
{
    if (!m_ShowSceneGraphWindow)
        return;

    ImGui::SetNextWindowSize(ImVec2(800, 1000), ImGuiCond_Once);
    if (!ImGui::Begin("Scene view", &m_ShowSceneGraphWindow, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }
    if (!m_SelectedNode || m_SelectedNode.expired())
        m_SelectedNode = m_Scene->GetSceneGraph()->GetRoot();

    if (ImGui::BeginChild("##tree", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        // Left side
        if (ImGui::BeginTable("##bg", 1, ImGuiTableFlags_RowBg))
        {
            uint32_t pushid_count = 0;
            alm::gfx::SceneGraph::Walker walker(*m_Scene->GetSceneGraph());
            while (walker)
            {
                alm::gfx::SceneGraphNode* node = *walker;
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
                if (node == m_SelectedNode.get())
                    tree_flags |= ImGuiTreeNodeFlags_Selected;
                if (node->GetChildrenCount() == 0)
                    tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool node_open = ImGui::TreeNodeEx("", tree_flags, "%s", node->GetName().c_str());

                // Make an additional PopID here in case the node was no open because it will not be done in the depth 'while'
                if (!node_open)
                    ImGui::PopID();

                if (ImGui::IsItemFocused())
                    m_SelectedNode = node->weak_from_this();

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
        if (const auto& node = m_SelectedNode)
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

                if (alm::has_any_flag(node->GetContentFlags(), alm::gfx::SceneContentFlags::Meshes))
                {
                    ImGui::SeparatorText("Mesh Bounds");
                    {
                        const alm::aabox3f& bbox = node->GetWorldBounds(alm::gfx::SceneContentType::Meshes);
                        ImGui::Text("Min");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##meshBboxMin", (float*)&(bbox.min), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::Text("Max");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##meshBboxMax", (float*)&(bbox.max), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    }
                }

                if (alm::has_any_flag(node->GetContentFlags(), alm::gfx::SceneContentFlags::SpotLights))
                {
                    ImGui::SeparatorText("Spot Light Bounds");
                    {
                        const alm::aabox3f& bbox = node->GetWorldBounds(alm::gfx::SceneContentType::SpotLights);
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
                    flagCell("Meshes", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::Meshes));
                    flagCell("DirLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::DirectionalLights));

                    // Second row
                    ImGui::TableNextRow();
                    flagCell("ShadowCasters", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::ShadowCasters));
                    flagCell("PointLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::PointLights));

                    // Third row
                    ImGui::TableNextRow();
                    flagCell("Animations", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::Animations));
                    flagCell("SpotLights", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::SpotLights));

                    // Four row
                    flagCell("Cameras", alm::has_any_flag(flags, alm::gfx::SceneContentFlags::Cameras));

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

void alm::fw::FrameworkUI::BuildRenderModesSettings()
{
    if (ImGui::CollapsingHeader("Render modes", ImGuiTreeNodeFlags_None))
    {
        std::vector<std::string> renderModes = m_RenderViewUI->GetRenderGraph()->GetRenderModes();
        int selectedIdx = 0;
        for (int i = 0; i < renderModes.size(); ++i)
        {
            if (renderModes[i] == m_RenderViewUI->GetRenderGraph()->GetCurrentRenderMode())
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

        FrameworkData.RenderMode = renderModes[selectedIdx];
    }
}

void alm::fw::FrameworkUI::BuildCameraSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
    {
        auto camera = m_RenderViewUI->GetCamera();

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
        if (m_CameraController)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + itemWidth / 2.f);
            float speed = m_CameraController->GetSpeed();
            if (ImGui::InputFloat("Speed", &speed, 0.f, 0.f, "%.3f", 0))
            {
                m_CameraController->SetSpeed(speed);
            }
        }
    }
}

void alm::fw::FrameworkUI::BuildDebugViewSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Debug view", ImGuiTreeNodeFlags_None))
    {
        ImGui::Checkbox("Mesh BBoxes", &FrameworkData.Debug.ShowMeshBBoxes);
        ImGui::SameLine(availWidth / 2);
        ImGui::Checkbox("Light BBoxes", &FrameworkData.Debug.ShowLightBBoxes);
    }
}

void alm::fw::FrameworkUI::BuildShadowmapSettings(float availWidth)
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (ImGui::CollapsingHeader("Shadowmap", ImGuiTreeNodeFlags_None))
    {
        auto shadowmapRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
        if (!shadowmapRS)
            return;

        auto lightingRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();

        ImGui::SetNextItemWidth(availWidth / 2);
        bool isEnabled = shadowmapRS->IsEnabled();
        if(ImGui::Checkbox("Enabled##Shadowmap", &isEnabled))
            shadowmapRS->SetEnabled(isEnabled);

        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        bool showShadowmap = lightingRS->GetShowShadowmap();
        if(ImGui::Checkbox("Show##Shadowmap", &showShadowmap))
            lightingRS->ShowShadowmap(showShadowmap);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(availWidth / 4);
        int depthBias = shadowmapRS->GetDepthBias();
        if(ImGui::InputInt("Depth Bias##Shadowmap", &depthBias))
            shadowmapRS->SetDepthBias(depthBias);

        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 4);
        float slopeBias = shadowmapRS->GetSlopeScaledDepthBias();
        if (ImGui::InputFloat("Slope scaled Depth Bias##Shadowmap", &slopeBias))
            shadowmapRS->SetSlopeScaledDepthBias(slopeBias);

        ImGui::Spacing();

        float itemWidth = availWidth / 5;
        ImGui::SetCursorPosX(style.ItemSpacing.x);

        ImGui::Text("Size");
        ImGui::SameLine();
        ImGui::SetCursorPosX(itemWidth + style.ItemSpacing.x);

        if (m_ShadowmapSizeCached.x < 0)
            m_ShadowmapSizeCached = shadowmapRS->GetSize();

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizex", (int*)&m_ShadowmapSizeCached.x, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizey", (int*)&m_ShadowmapSizeCached.y, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        if (ImGui::Button("Reset", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
            m_ShadowmapSizeCached = shadowmapRS->GetSize();

        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
        {
            FrameworkData.ShadowmapSize = m_ShadowmapSizeCached;
        }
    }
}

void alm::fw::FrameworkUI::BuildDirectionalLightSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Directional light"))
    {
        const float minAzimuth = -180.0;
        const float maxAzimuth = 180.0;
        const float minElevation = -90.0;
        const float maxElevation = 90.0;

        FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
            "Azimuth", ImGuiDataType_Float, &FrameworkData.SunParams.AzimuthDeg, &minAzimuth, &maxAzimuth, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
            "Elevation", ImGuiDataType_Float, &FrameworkData.SunParams.ElevationDeg, &minElevation, &maxElevation, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        FrameworkData.SunParamsUpdated |= ImGui::ColorEdit3(
            "Color", &FrameworkData.SunParams.Color.x, ImGuiColorEditFlags_Float);
        FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
            "Irradiance", &FrameworkData.SunParams.Irradiance, 0.f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
            "Angular Size", &FrameworkData.SunParams.AngularSizeDeg, 0.0f, 20.f);
    }
}

void alm::fw::FrameworkUI::BuildAmbientLightSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Ambient"))
    {
        FrameworkData.AmbientParamsUpdated |= ImGui::SliderFloat("Intensity", &FrameworkData.AmbientParams.Intensity, 0.f, 1.f);
        FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Sky Color", &FrameworkData.AmbientParams.SkyColor.x, ImGuiColorEditFlags_Float);
        FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Ground Color", &FrameworkData.AmbientParams.GroundColor.x, ImGuiColorEditFlags_Float);
    }
}

void alm::fw::FrameworkUI::BuildSkySettings(float availWidth)
{
    auto simpleSky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
    if (simpleSky)
    {
        if (ImGui::CollapsingHeader("Simple Sky"))
        {
            bool isEnabled = simpleSky->IsEnabled();
            ImGui::Checkbox("Enabled##Sky", &isEnabled);
            simpleSky->SetEnabled(isEnabled);

            ImGui::ColorEdit3("Sky color##Sky", &FrameworkData.SimpleSkyParams.skyColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Horizon color##Sky", &FrameworkData.SimpleSkyParams.horizonColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Ground color##Sky", &FrameworkData.SimpleSkyParams.groundColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Brightness scaler", &FrameworkData.SimpleSkyParams.brightness, 0.f, 10.f);

            float horizonSizeRad = glm::radians(FrameworkData.SimpleSkyParams.horizonSize);
            if (ImGui::SliderAngle("Horizon size", &horizonSizeRad, 0.f, 90.f))
            {
                FrameworkData.SimpleSkyParams.horizonSize = glm::degrees(horizonSizeRad);
            }
            float glowSizeRad = glm::radians(FrameworkData.SimpleSkyParams.glowSize);
            if (ImGui::SliderAngle("Glow size", &glowSizeRad, 0.f, 90.f))
            {
                FrameworkData.SimpleSkyParams.glowSize = glm::degrees(glowSizeRad);
            }

            ImGui::SliderFloat("Glow intensity", &FrameworkData.SimpleSkyParams.glowIntensity, 0.f, 1.f);
            ImGui::SliderFloat("Glow sharpness", &FrameworkData.SimpleSkyParams.glowIntensity, 1.f, 10.f);
            ImGui::InputFloat("Max radiance", &FrameworkData.SimpleSkyParams.maxLightRadiance);
        }
    }

    auto sky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::SkyRenderStage>();
    if (sky)
    {
        if (ImGui::CollapsingHeader("Sky"))
        {
            ImGui::Checkbox("Enabled##Sky", &FrameworkData.SkyEnabled);
            ImGui::SliderFloat("Mie Anisotropy##Sky", &FrameworkData.SkyParams.MieAnisotropy, 0.f, 1.f);
            ImGui::InputScalar("Atmos interations##Sky", ImGuiDataType_U32, &FrameworkData.SkyParams.NumSteps);
            ImGui::InputScalar("Light interations##Sky", ImGuiDataType_U32, &FrameworkData.SkyParams.NumLightSteps);
        }
    }
}

void alm::fw::FrameworkUI::BuildsCloudsSettings()
{
    auto cloudsRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::CloudsRenderStage>();
    if(cloudsRS && ImGui::CollapsingHeader("Clouds"))
    {
        float2 velDir = FrameworkData.CloudsParams.WindVelocity;
        float velMag = glm::length(velDir);
        float velYaw = 0.f;
        if (velMag > 0.0001f)
        {
            velDir /= velMag;
            velYaw = atan2(velDir.x, velDir.y);
        }
        bool windUpdated = false;
        windUpdated |= ImGui::SliderAngle("Wind Velocity Yaw##Clouds", &velYaw, -180.f, 180.f);
        windUpdated |= ImGui::InputFloat("Wind Magnitude", &velMag);
        if (windUpdated)
        {
            FrameworkData.CloudsParams.WindVelocity.x = glm::sin(velYaw);
            FrameworkData.CloudsParams.WindVelocity.y = glm::cos(velYaw);
            FrameworkData.CloudsParams.WindVelocity *= velMag;
        }

        ImGui::SliderFloat("Scale##Clouds", &FrameworkData.CloudsParams.CloudsScale, 0.f, 1.f, "%.6f",
            ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Coverage##Clouds", &FrameworkData.CloudsParams.CloudsCoverage, 0.f, 1.f);
        ImGui::SliderFloat("Absorption##Clouds", &FrameworkData.CloudsParams.AbsorptionCoeff, 0.f, 1.f);
        ImGui::InputFloat("Clouds Bottom##Clouds", &FrameworkData.CloudsParams.CloudsLayerMin);
        ImGui::InputFloat("Clouds Top##Clouds", &FrameworkData.CloudsParams.CloudsLayerMax);
        ImGui::InputFloat("Fade Distance##Clouds", &FrameworkData.CloudsParams.CloudsFadeDistance);
        ImGui::InputFloat("Earth Radius##Clouds", &FrameworkData.CloudsParams.EarthRadius);
        ImGui::InputScalar("Density interations##Clouds", ImGuiDataType_U32, &FrameworkData.CloudsParams.CloudRaymarchIterations);
        ImGui::InputScalar("Light interations##Clouds", ImGuiDataType_U32, &FrameworkData.CloudsParams.LightRaymarchIterations);

        if (ImGui::Button("Open Shape Texture"))
        {
            auto tex = cloudsRS->GetCloudsShapeTexture();
            AddTextureWindow(tex->GetDebugName(), tex);
        }
    }
}

void alm::fw::FrameworkUI::BuildMaterialChannelsSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Material channels"))
    {
        if (ImGui::RadioButton("Disabled##matDisabled", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;
        if (ImGui::RadioButton("Base Color##matBaseColor", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor;
        if (ImGui::RadioButton("Metalness##matMetalness", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness;
        if (ImGui::RadioButton("Anisotropy##matAnisotropy", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy;
        if (ImGui::RadioButton("Roughness##matRoughness", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness;
        if (ImGui::RadioButton("Scattering##matScattering", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering;
        if (ImGui::RadioButton("Translucency##matTranslucency", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency;
        if (ImGui::RadioButton("Normal Map##matNormalMap", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap;
        if (ImGui::RadioButton("Occlusion Map##matOcclusionMap", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap;
        if (ImGui::RadioButton("Emissive##matEmissive", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive;
        if (ImGui::RadioButton("Specular F0##matSpecularF0", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0;
    }
}

void alm::fw::FrameworkUI::BuildSSAOSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("SSAO"))
    {
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Enabled##SSAO", &FrameworkData.SSAO.Enabled);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Show##SSAO", &FrameworkData.SSAO.View);

        ImGui::InputFloat("Radius##SSAO", &FrameworkData.SSAO.Radius);
        ImGui::InputFloat("Power##SSAO", &FrameworkData.SSAO.Power);
        ImGui::InputFloat("Bias##Bias", &FrameworkData.SSAO.Bias);
    }
}

void alm::fw::FrameworkUI::BuildBloomSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Bloom"))
    {
        ImGui::Checkbox("Enabled##Bloom", &FrameworkData.Bloom.Enabled);
        ImGui::InputFloat("Radius##Bloom", &FrameworkData.Bloom.Radius);
        ImGui::InputFloat("Strength##Bloom", &FrameworkData.Bloom.Strength);

        // Calc whats the max mip chain
        alm::rhi::TextureHandle tex = m_RenderViewUI->GetRenderGraph()->GetTexture("SceneColor");
        int maxMips = static_cast<int>(std::floor(std::log2((std::max)(tex->GetDesc().width, tex->GetDesc().height))));
        ImGui::SliderInt("MaxMip##Bloom", &FrameworkData.Bloom.MaxMip, 1, maxMips);
    }
}

void alm::fw::FrameworkUI::BuildRenderStagesWindow()
{
    if (!m_ShowRenderStages)
        return;

    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();
    const float parentHeight = ImGui::GetIO().DisplaySize.y;
    const float windowHeight = parentHeight * 0.9f;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

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

void alm::fw::FrameworkUI::BuildTonemappingSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Tonemapping"))
    {
        ImGui::Checkbox("Enabled##Tonemapping", &FrameworkData.Tonemapping.Enabled);

        alm::rhi::ColorSpace colorSpace = GetDeviceManager()->GetColorSpace();
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
        ImGui::SliderScalar("Min log luminance", ImGuiDataType_Float, &FrameworkData.Tonemapping.MinLogLuminance,
            &minLogLuminance, &maxLogLuminance, "%.3f");

        const float minLogRange = 0.f;
        const float maxLogRange = 36.f;
        ImGui::SliderScalar("Log luminance range", ImGuiDataType_Float, &FrameworkData.Tonemapping.LogLuminanceRange,
            &minLogRange, &maxLogRange, "%.3f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const float minLum = exp2(FrameworkData.Tonemapping.MinLogLuminance);
        const float maxLum = minLum * std::exp2(FrameworkData.Tonemapping.LogLuminanceRange);
        ImGui::Text("Luminance [%1.6f .. %1.6f]", minLum, maxLum);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::SliderFloat("Middle Gray Nits", &FrameworkData.Tonemapping.MiddleGrayNits, 0.f, 1000.f);
        ImGui::SliderFloat("Paper White Nits", &FrameworkData.Tonemapping.PaperWhiteNits, 0.f, 1000.f);

        ImGui::Spacing();

        ImGui::InputFloat("Dark to light adaptation speed", &FrameworkData.Tonemapping.AdaptationUpSpeed);
        ImGui::InputFloat("Light to dark adaptation speed", &FrameworkData.Tonemapping.AdaptationDownSpeed);

        ImGui::Spacing();

        m_ShowLuminanceHistogram |= ImGui::Button("View Histogram");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float minExposureBias = 0.f;
        const float maxExposureBias = 1.f;
        ImGui::SliderScalar("SDR Exposure Bias", ImGuiDataType_Float, &FrameworkData.Tonemapping.SdrExposureBias,
            &minExposureBias, &maxExposureBias, "%.3f");
    }
}

void alm::fw::FrameworkUI::BuildLumninanceHistogram()
{
    auto tonemappingRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::ToneMappingRenderStage>();

    if (!tonemappingRS || !m_ShowLuminanceHistogram)
        return;

    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    if (!m_LumHistogramBufferTicket.IsValid())
    {
        m_LumHistogramBufferTicket = renderGraph->RequestBufferView(
            tonemappingRS->GetType(), alm::gfx::RenderGraph::AccessMode::Write, renderGraph->GetBufferHandle("LuminanceHistogram"));
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

    alm::gfx::ToneMappingRenderStage::Stats stats = tonemappingRS->GetStats();

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
                tw.blueChannel = true;
                tw.alphaChannel = false;
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

std::string alm::fw::FrameworkUI::OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters)
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

std::string alm::fw::FrameworkUI::SaveFileNativeDialog(const std::string& filename)
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