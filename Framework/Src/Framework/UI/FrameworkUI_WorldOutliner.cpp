// FrameworkUI "World Outliner" window (View > World Outliner), including the
// viewport node picking and the per-leaf-type inspector builders.
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/SceneLights.h"
#include "Gfx/RenderView.h"
#include "Gfx/Camera.h"
#include <SDL3/SDL.h>
#include <imgui/imgui_internal.h>   // ImGui::IsMouseDragPastThreshold (BuildViewportPicking)

namespace
{
    using alm::fw::TextRightAligned;

    void PropertyRowText(const char* label, const char* value, std::function<void()> onClick = nullptr)
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
        if (onClick)
        {
            // A button clearly reads as clickable (a disabled InputText suggests the text is editable).
            // ImGui centers button labels, so the text is drawn manually on top, left-aligned.
            const ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Button("##value", ImVec2(-FLT_MIN, 0.f)))
                onClick();
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(buttonPos.x + ImGui::GetStyle().FramePadding.x, buttonPos.y));
            ImGui::TextUnformatted(value);
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##value", (char*)value, strlen(value) + 1, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
        }
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

    void BuildMeshInstanceLeaf(alm::fw::FrameworkUI* ui, const alm::gfx::MeshInstance* leaf)
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
                PropertyRowText("Name", mat->GetName().c_str(), [ui, mat]() { ui->OpenMaterialPanel(mat); });
                ImGui::EndTable();
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

    void BuildHeightmapLeaf(const alm::gfx::SceneHeightmap*)
    {
        // TODO
    }
} // anonymous namespace

void alm::fw::FrameworkUI::OpenWorldOutliner()
{
    m_ShowWorldOutliner = true;

    if (!m_SelectedNode || m_SelectedNode.expired())
        return;

    m_ExpandAncestors.clear();
    for (auto parent = m_SelectedNode->GetParent(); parent; parent = parent->GetParent())
        m_ExpandAncestors.push_back(parent);
    m_ScrollToSelectedNode = true;
}

void alm::fw::FrameworkUI::BuildWorldOutliner()
{
    if (!m_ShowWorldOutliner)
        return;

    ImGui::SetNextWindowSize(ImVec2(1300, 1000), ImGuiCond_Once);
    if (!ImGui::Begin("World Outliner", &m_ShowWorldOutliner, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginChild("##world_outliner_panel_left", ImVec2(450, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        // Left side
        if (ImGui::BeginTable("##tree", 1, ImGuiTableFlags_RowBg))
        {
            alm::gfx::SceneGraph::Walker walker(*m_Scene->GetSceneGraph());
            while (walker)
            {
                alm::gfx::SceneGraphNode* node = *walker;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID((const void*)node);

                // Force-expand ancestors so the selected row is actually rendered
                if (m_ScrollToSelectedNode && std::find(m_ExpandAncestors.begin(), m_ExpandAncestors.end(), node) != m_ExpandAncestors.end())
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);

                ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
                tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;// Standard opening mode as we are likely to want to add selection afterwards
                tree_flags |= ImGuiTreeNodeFlags_NavLeftJumpsToParent;  // Left arrow support
                tree_flags |= ImGuiTreeNodeFlags_SpanFullWidth;         // Span full width for easier mouse reach
                tree_flags |= ImGuiTreeNodeFlags_DrawLinesToNodes;      // Always draw hierarchy outlines
                tree_flags |= ImGuiTreeNodeFlags_DefaultOpen;
                if (m_SelectedNode && m_SelectedNode.get() == node)
                    tree_flags |= ImGuiTreeNodeFlags_Selected;
                if (node->GetChildrenCount() == 0)
                    tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool node_open = ImGui::TreeNodeEx("", tree_flags, "%s", node->GetName().c_str());

                // Make an additional PopID here in case the node was no open because it will not be done in the depth 'while'
                if (!node_open)
                    ImGui::PopID();

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    SetSelectedNode(node->weak_from_this());

                if (m_ScrollToSelectedNode && node == m_SelectedNode.get())
                {
                    ImGui::SetScrollHereY(0.5f);   // centra la fila; 0.f = arriba
                    m_ScrollToSelectedNode = false;
                    m_ExpandAncestors.clear();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    auto camera = m_RenderViewUI->GetCamera();
                    const alm::aabox3f& bounds = m_SelectedNode->GetWorldBounds(gfx::SceneContentType::Meshes);
                    camera->Frame(bounds);
                }

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
            m_ScrollToSelectedNode = false;
        }
    }
    ImGui::EndChild();

    // Right side
    ImGui::SameLine();
    if (ImGui::BeginChild("##world_outliner_panel_right", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar))
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
                    BuildMeshInstanceLeaf(this, alm::checked_cast<const alm::gfx::MeshInstance*>(leaf));
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
                case alm::gfx::SceneGraphLeaf::Type::Heightmap:
                    BuildHeightmapLeaf(alm::checked_cast<const alm::gfx::SceneHeightmap*>(leaf));
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

void alm::fw::FrameworkUI::BuildViewportPicking()
{
    // Deselects and raycasts the 3D viewport at the given mouse position (window-relative).
    auto trySelectAt = [this](const float2& mousePos) -> std::optional<gfx::RaycastHit>
    {
        SetSelectedNode(nullptr);

        auto sceneGraph = m_Scene ? m_Scene->GetSceneGraph() : nullptr;
        auto camera = m_RenderViewUI ? m_RenderViewUI->GetCamera() : nullptr;
        if (!sceneGraph || !camera)
            return {};

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        auto [origin, dir] = camera->ScreenToWorldRay(
            uint2{ (uint32_t)mousePos.x, (uint32_t)mousePos.y },
            uint2{ (uint32_t)displaySize.x, (uint32_t)displaySize.y });

        gfx::RaycastHit hit{};
        if (!sceneGraph->Raycast(origin, dir, hit))
            return {};

        return hit;
    };

    // Right-click on the 3D viewport: the dockspace PassthruCentralNode registers a hit-test hole
    // so the viewport counts as "void" (no hovered ImGui window). Ignore right-drag releases
    // (camera controls). A double right-click opens the World Outliner on the selected node.
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Right) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        float mouseX = 0.f, mouseY = 0.f;
        SDL_GetMouseState(&mouseX, &mouseY); // window-relative, same space as ScreenToWorldRay
        auto hitResult = trySelectAt({ (int)mouseX, (int)mouseY });
        if (hitResult)
        {
            SetSelectedNode(hitResult->Node->weak_from_this());

            // NOTE: IsMouseDoubleClicked() is only valid on the press frame of the second click
            // (io.MouseClickedCount is zeroed every frame and only filled on clicks). On this
            // release frame, read the persistent click counter instead (stays valid after release).
            if (ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Right] >= 2)
                OpenWorldOutliner();

            m_ScrollToSelectedNode = true;
        }
        else
        {
            SetSelectedNode(nullptr);
        }
    }
}

