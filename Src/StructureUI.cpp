#include "StructureUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include <format>
#include <Windows.h>
#include <SDL3/SDL.h>

namespace
{
void PropertyRowText(const char* label, const char* value)
{
    ImGui::TableNextRow();
    // Label
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    // Value (fake input)
    ImGui::TableNextColumn();
    ImGui::PushID(label);
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
    ImGui::TextUnformatted(label);
    // Value
    ImGui::TableNextColumn();
    ImGui::PushID(label);
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputInt("##value", &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    ImGui::PopID();
}

} // anonymous namespace

StructureUI::StructureUI(SDL_Window* window) : 
    m_Window{ window },
    m_ShowSceneWindow{ false },
    m_SelectedNode{ nullptr }
{}

void StructureUI::BuildUI()
{
    if (!m_Data.ShowUI)
        return;

    ImGuiIO const& io = ImGui::GetIO();
    BeginFullScreenWindow();

    if (!m_RenderView->GetScene())
    {
        DrawCenteredText("Click File->Open to open a scene file");
    }

    // Show FPS
    {
        std::string fps = std::format(" {:.3f} FPS", m_Data.FPS);

        ImVec2 textSize = ImGui::CalcTextSize(fps.c_str());
        ImGui::SetCursorPosX(0.f);
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - textSize.y);
        ImGui::TextUnformatted(fps.c_str());
    }

    EndFullScreenWindow();

    BuildMainMenu();

    if(m_ShowSceneWindow)
        BuildSceneWindow(&m_ShowSceneWindow);
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
            {
                m_ShowSceneWindow = !m_ShowSceneWindow;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
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
    ImGui::SetNextWindowSize(ImVec2(430, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Dear ImGui Demo", p_open, ImGuiWindowFlags_None) || !scene || !scene->GetSceneGraph() || !scene->GetSceneGraph()->GetRoot())
    {
        ImGui::End();
        return;
    }
    if (!m_SelectedNode)
        m_SelectedNode = scene->GetSceneGraph()->GetRoot().get();

    ImGuiTextFilter Filter;
    if (ImGui::BeginChild("##tree", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
    {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_Tooltip);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
        if (ImGui::InputTextWithHint("##Filter", "incl,-excl", Filter.InputBuf, std::size(Filter.InputBuf), ImGuiInputTextFlags_EscapeClearsAll))
            Filter.Build();
        ImGui::PopItemFlag();

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
        ImGui::EndChild();

        // Right side
        ImGui::SameLine();
        ImGui::BeginGroup(); // Lock X position
        if (auto* node = m_SelectedNode)
        {
            ImGui::Text("%s", node->GetName().c_str());
            ImGui::Separator();

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
        ImGui::EndGroup();
    }
    ImGui::End();
}

void StructureUI::BuildMeshInstanceLeaf(const st::gfx::MeshInstance* leaf)
{
    ImGui::SeparatorText("Mesh Instance");
    const auto& mesh = leaf->GetMesh();
    const st::rhi::PrimitiveTopology topo = mesh->GetPrimitiveTopology();

    ImGui::BeginTable("MeshProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
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

    ImGui::EndTable();
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