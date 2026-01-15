#include "StructureUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderStage.h"
#include "RHI/Device.h"
#include <format>
#include <Windows.h>
#include <SDL3/SDL.h>
#include <imgui/imgui_internal.h>
#include <sstream>

namespace
{

void TextRightAligned(const char* text)
{
    float textWidth = ImGui::CalcTextSize(text).x;
    float colWidth = ImGui::GetColumnWidth();
    float cursorX = ImGui::GetCursorPosX();

    ImGui::SetCursorPosX(cursorX + colWidth - textWidth);
    ImGui::TextUnformatted(text);
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

struct RSDepResult
{
    bool hovered;
    bool clicked;
};

RSDepResult ShowRSDep(const char* label, const char* value, bool selected, int id)
{
    RSDepResult ret;

    ImGui::PushID(id);

    // Label
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.0f, 40.0f);

    float startX = ImGui::GetCursorPosX();
    float width = 300.0f;
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
    ImGui::SameLine(startX + width + 8.0f);
    ret.clicked = ImGui::Button("...", ImVec2(24, 0));

    ImGui::PopID();
    return ret;
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

const char* GetTextureDimensionText(st::rhi::TextureDimension dim)
{
    switch (dim)
    {
    case st::rhi::TextureDimension::Texture1D: return "Texture1D";
    case st::rhi::TextureDimension::Texture1DArray: return "Texture1DArray";
    case st::rhi::TextureDimension::Texture2D: return "Texture2D";
    case st::rhi::TextureDimension::Texture2DArray: return "Texture2DArray";
    case st::rhi::TextureDimension::TextureCube: return "TextureCube";
    case st::rhi::TextureDimension::TextureCubeArray: return "TextureCubeArray";
    case st::rhi::TextureDimension::Texture2DMS: return "Texture2DMS";
    case st::rhi::TextureDimension::Texture2DMSArray: return "Texture2DMSArray";
    case st::rhi::TextureDimension::Texture3D: return "Texture3D";
    default: return "Unknown";
    }
}

void BuildTexture(const st::gfx::LoadedTexture& diffuseTex)
{
    ImGui::SeparatorText("Diffuse texture");
    auto desc = diffuseTex.texture->GetDesc();

    ImGui::BeginTable("texId", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    PropertyRowText("id", diffuseTex.id.c_str());
    ImGui::EndTable();

    ImGui::BeginTable("##texDesc", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
    ImGui::TableSetupColumn("txt0", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("val0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("txt1", ImGuiTableColumnFlags_WidthFixed, 120);
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

StructureUI::StructureUI(st::weak<st::gfx::RenderView> renderView, SDL_Window* window, st::gfx::DeviceManager* deviceManager) :
    m_Window{ window },
    m_DeviceManager{ deviceManager },
    m_RenderView{ renderView },
    m_ShowSceneWindow{ false },
    m_SelectedNode{ nullptr },
    m_ShowResourcesWindow{ false },
    m_ShowRenderStages{ false }
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

    if (m_ShowSceneWindow)
        BuildSceneWindow(&m_ShowSceneWindow);

    if (m_ShowResourcesWindow)
        BuildResourcesWindow(&m_ShowResourcesWindow);

    if (m_ShowRenderStages)
        BuildRenderStagesWindow();

    for (auto it = m_RSTextureViews.begin(); it != m_RSTextureViews.end();)
    {
        if (BuildRSTexView(&(*it)))
        {
            it++;
        }
        else
        {
            m_RenderView->ReleaseTextureView(it->ticket);
            it = m_RSTextureViews.erase(it);
        }        
    }
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
}

void StructureUI::BuildMenuFile()
{
#if 0
    if (ImGui::MenuItem("New")) {}
    if (ImGui::MenuItem("Open", "Ctrl+O")) 
    {
        std::string filename = OpenFileNativeDialog({}, { { "GlTF", "*.gltf" } });
        if (!filename.empty() && m_RequestLoadFile)
        {
            m_RequestLoadFile(filename.c_str());
        }
    }
    if (ImGui::BeginMenu("Open Recent"))
    {
        ImGui::MenuItem("fish_hat.c");
        ImGui::MenuItem("fish_hat.inl");
        ImGui::MenuItem("fish_hat.h");
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {}
    if (ImGui::MenuItem("Save As..")) {}

    ImGui::Separator();

    if (ImGui::BeginMenu("Options"))
    {
        static bool enabled = true;
        ImGui::MenuItem("Enabled", "", &enabled);
        ImGui::BeginChild("child", ImVec2(0, 60), ImGuiChildFlags_Borders);
        for (int i = 0; i < 10; i++)
            ImGui::Text("Scrolling Text %d", i);
        ImGui::EndChild();
        static float f = 0.5f;
        static int n = 0;
        ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
        ImGui::InputFloat("Input", &f, 0.1f);
        ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Colors"))
    {
        float sz = ImGui::GetTextLineHeight();
        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
            ImGui::Dummy(ImVec2(sz, sz));
            ImGui::SameLine();
            ImGui::MenuItem(name);
        }
        ImGui::EndMenu();
    }

    // Here we demonstrate appending again to the "Options" menu (which we already created above)
    // Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
    // In a real code-base using it would make senses to use this feature from very different code locations.
    if (ImGui::BeginMenu("Options")) // <-- Append!
    {
        static bool b = true;
        ImGui::Checkbox("SomeOption", &b);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Disabled", false)) // Disabled
    {
        IM_ASSERT(0);
    }
    if (ImGui::MenuItem("Checked", NULL, true)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Quit", "Alt+F4")) {}
#else

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
#endif
}

void StructureUI::BuildSceneWindow(bool* p_open)
{
    auto scene = m_RenderView->GetScene();
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Scene view", p_open, ImGuiWindowFlags_None) || !scene || !scene->GetSceneGraph() || !scene->GetSceneGraph()->GetRoot())
    {
        ImGui::End();
        return;
    }
    if (!m_SelectedNode)
        m_SelectedNode = scene->GetSceneGraph()->GetRoot().get();

    ImGuiTextFilter Filter;
    if (ImGui::BeginChild("##tree", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        // Left side
        if (ImGui::BeginTable("##bg", 1, ImGuiTableFlags_RowBg))
        {
            uint32_t pushid_count = 0;
            st::gfx::SceneGraph::Walker walker(*scene->GetSceneGraph());
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
                //if (node->DataMyBool == false)
                //    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                bool node_open = ImGui::TreeNodeEx("", tree_flags, "%s", node->GetName().c_str());
                //if (node->DataMyBool == false)
                //    ImGui::PopStyleColor();
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
                // Make an additional PopID here in case the node was no open because it was nor done in the depth 'while'
                if (!node_open)
                    ImGui::PopID();
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
                    const st::gfx::Transform& localTransform = node->GetLocalTransform();
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
                    const st::gfx::Transform worldTransform{ node->GetWorldTransform() };
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

                if (node->HasBounds())
                {
                    ImGui::SeparatorText("Bounds");
                    {
                        const st::math::aabox3f& bbox = node->GetWorldBounds();
                        ImGui::Text("Min");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##bboxMin", (float*)&(bbox.min), "%.3f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::Text("Max");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::InputFloat3("##bboxMax", (float*)&(bbox.max), "%.3f", ImGuiInputTextFlags_ReadOnly);
                    }
                }

                ImGui::SeparatorText("ContentFlags");
                {
                    const st::gfx::SceneContentFlags flags = node->GetContentFlags();
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
                    flagCell("OpaqueMeshes", st::has_flag(flags, st::gfx::SceneContentFlags::OpaqueMeshes));
                    flagCell("Lights", st::has_flag(flags, st::gfx::SceneContentFlags::Lights));

                    // Second row
                    ImGui::TableNextRow();
                    flagCell("AlphaTestedMeshes", st::has_flag(flags, st::gfx::SceneContentFlags::AlphaTestedMeshes));
                    flagCell("Cameras", st::has_flag(flags, st::gfx::SceneContentFlags::Cameras));

                    // Third row
                    ImGui::TableNextRow();
                    flagCell("BlendedMeshes", st::has_flag(flags, st::gfx::SceneContentFlags::BlendedMeshes));
                    flagCell("Animations", st::has_flag(flags, st::gfx::SceneContentFlags::Animations));

                    ImGui::EndTable();
                }
            }

            const auto* leaf = node->GetLeaf() ? node->GetLeaf().get() : nullptr;
            if (leaf)
            {
                switch (leaf->GetType())
                {
                case st::gfx::SceneGraphLeaf::Type::MeshInstance:
                    BuildMeshInstanceLeaf(st::checked_cast<const st::gfx::MeshInstance*>(leaf));
                    break;
                case st::gfx::SceneGraphLeaf::Type::Camera:
                    assert(0);
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
        st::rhi::Device* device = m_DeviceManager->GetDevice();


        ImGui::End();
        return;
    }

    ImGui::End();
}

void StructureUI::BuildRenderStagesWindow()
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!ImGui::Begin(m_RenderView->GetName().c_str(), &m_ShowRenderStages, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    int propid = 0;
    std::string newHoveredId;
    for (int rs_idx = 0; rs_idx < m_RenderView->GetNumRenderStages(); ++rs_idx)
    {
        const auto* rs = m_RenderView->GetRenderStage(rs_idx);
        if (ImGui::CollapsingHeader(rs->renderStage->GetDebugName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!rs->reads.empty())
            {
                ImGui::SeparatorText("Reads");

                for (const auto& dep : rs->reads)
                {
                    const auto* declTex = dep.declTex;
                    bool selected = (m_RenderStageIOHoveredId == declTex->id);

                    RSDepResult result = ShowRSDep("id", declTex->id.c_str(), selected, propid++);
                    if (result.hovered)
                    {
                        newHoveredId = declTex->id;
                    }
                    if (result.clicked)
                    {
                        AddRenderStageTextureView(rs->renderStage.get(), st::gfx::RenderView::AccessMode::Read, declTex->id);
                    }
                }
            }

            if (!rs->writes.empty())
            {
                ImGui::SeparatorText("Writes");

                for (const auto& dep : rs->writes)
                {
                    const auto* declTex = dep.declTex;
                    bool selected = (m_RenderStageIOHoveredId == declTex->id);
                    RSDepResult result = ShowRSDep("id", declTex->id.c_str(), selected, propid++);
                    if (result.hovered)
                    {
                        newHoveredId = declTex->id;
                    }
                    if (result.clicked)
                    {
                        AddRenderStageTextureView(rs->renderStage.get(), st::gfx::RenderView::AccessMode::Write, declTex->id);
                    }
                }
            }
        }
    }
    m_RenderStageIOHoveredId = newHoveredId;

    ImGui::End();
}

bool StructureUI::BuildRSTexView(RenderStageTextureView* rsTexView)
{
    bool isOpen = true;

    st::rhi::TextureHandle tex = m_RenderView->GetTextureView(rsTexView->ticket);
    if (!tex)
        return isOpen;

    std::stringstream title;
    title << rsTexView->renderStage->GetDebugName() << " - ";
    if (rsTexView->accessMode == st::gfx::RenderView::AccessMode::Read)
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
        ShowPropertyText("Format", labelWidth, st::rhi::GetFormatString(texDesc.format), labelHeight, ++id);

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

void StructureUI::BuildMeshInstanceLeaf(const st::gfx::MeshInstance* leaf)
{
    const auto& mesh = leaf->GetMesh();

    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const st::rhi::PrimitiveTopology topo = mesh->GetPrimitiveTopology();

        ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const char* topoStr = nullptr;
        switch (topo)
        {
        case st::rhi::PrimitiveTopology::PointList:
            PropertyRowText("Primitive type", "PointList");
            break;
        case st::rhi::PrimitiveTopology::TriangleList:
            PropertyRowText("Primitive type", "TriangleList");
            break;
        default:
            assert(0);
        }
    
        int primCount = st::rhi::GetPrimitiveCount(mesh->GetIndexCount(), topo);
        PropertyRowInt("Primitive count", primCount);

        PropertyRowInt("Index count", mesh->GetIndexCount());

        int vertexCount = mesh->GetVertexBuffer()->GetDesc().sizeBytes / mesh->GetVertexFormat().VertexStride;
        PropertyRowInt("Vertex count", vertexCount);

        PropertyRowInt("IB size bytes", mesh->GetIndexBuffer()->GetDesc().sizeBytes);
        PropertyRowInt("VB size bytes", mesh->GetVertexBuffer()->GetDesc().sizeBytes);

        ImGui::EndTable();

        ImGui::SeparatorText("Vertex format");
        {
            const st::gfx::Mesh::VertexFormat& vertexFormat = mesh->GetVertexFormat();
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
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            PropertyRowText("Name", mat->GetName().c_str());
            ImGui::EndTable();

            const auto& baseTex = mat->GetBaseColorTexture();
            if (baseTex)
            {
                BuildTexture(*baseTex);
            }
        }
    }
}

void StructureUI::AddRenderStageTextureView(st::gfx::RenderStage* renderStage, st::gfx::RenderView::AccessMode accessMode, const std::string& id)
{
    // Check if it already exists
    for (auto& entry : m_RSTextureViews)
    {
        if (entry.renderStage == renderStage && entry.accessMode == accessMode && entry.id == id)
        {
            // TODO: focus
            return;
        }
    }

    auto ticket = m_RenderView->RequestTextureView(renderStage, accessMode, id);

    m_RSTextureViews.emplace_back(renderStage, accessMode, id, ticket);
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