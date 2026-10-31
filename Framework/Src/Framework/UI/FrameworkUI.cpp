#include "Framework/FrameworkPCH.h"
#include "Framework/CameraController.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/SceneLights.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/MaterialManager.h"
#include "Gfx/Camera.h"
#include "Gfx/TextureCache.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/RenderStages/WBOITAccumRenderStage.h"
#include "Gfx/RenderStages/WireframeRenderStage.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderStages/GridRenderStage.h"
#include "Gfx/RenderStages/ObjectOutlineRenderStage.h"
#include "Gfx/RenderStages/ExposureRenderStage.h"
#include "RHI/Device.h"
#include "ImGuizmo/ImGuizmo.h"
#include <imgui/imgui_internal.h> // For ImGui::GetCurrentWindow()
#include <SDL3/SDL.h>
#include <commdlg.h>
#include "imgui_memory_editor.h"

namespace
{

    // ImGuizmo gizmo state (for the selected node). File-local on purpose: it only exists
    // to keep ImGuizmo out of the public FrameworkUI header.
    ImGuizmo::OPERATION g_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE g_GizmoMode = ImGuizmo::WORLD;
    bool g_GizmoUseSnap = false;
    float g_GizmoSnap[3] = { 1.f, 1.f, 1.f };
    bool g_GizmoDragging = false;
    alm::gfx::Transform g_GizmoDragStartTransform;

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
    if (m_SelectedNode && m_SelectedNode.expired())
        SetSelectedNode(nullptr);
    // ESC deselects the node, but not while the gizmo is being dragged: there ESC cancels the
    // drag itself (handled in BuildGizmo) and the selection is kept.
    // NOTE: IsUsingAny() must be used here: IsUsing() compares against ImGuizmo's own ID stack
    // and always returns false when queried outside the gizmo's PushID scope.
    if (m_SelectedNode && ImGui::IsKeyChordPressed(ImGuiKey_Escape) && !ImGuizmo::IsUsingAny())
        SetSelectedNode(nullptr);

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        m_ShowSettings = true;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W))
        m_ShowWorldOutliner = true;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_R))
        m_ShowRenderStages = true;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_M))
        m_ShowMaterials = true;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_G))
        SetGridVisible(!IsGridVisible());
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_W))
        SwitchWireframeRender();

    BuildMainMenu();
    BuildBottomBar();

    BuildSettingsWindow();
    BuildWorldOutliner();
    BuildRenderStagesWindow();
    BuildMaterialsWindow();
    BuildAtmospherePanel();
    BuildStatsWindow();

    BuildGPUCPUGraphWindow();
    BuildStagesGPUGraphWindow();
    BuildStagesCPUGraphWindow();

    BuildViewportPicking();

    BuildLumninanceHistogram();
    BuildTextureWindows();
    BuildRSViews();

    ShowPendingErrorModal();

    BuildGizmo(GetDockspaceId());

    if (m_ShowImGuiDebugger)
    {
        ImGui::ShowMetricsWindow(&m_ShowImGuiDebugger);
    }
}

void alm::fw::FrameworkUI::BuildGizmo(ImGuiID dockspaceId)
{
    if (!m_SelectedNode)
        return;

    ImGuizmo::SetOrthographic(false);
    // Keep the gizmo axes fixed to the world axes (+X/+Y/+Z): without this ImGuizmo flips
    // each axis towards the camera when it sits on the negative half of that axis.
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::BeginFrame();
    ImGuizmo::PushID(m_SelectedNode.get());
    ImGuizmo::Enable(true);

    // Gizmo hotkeys (ignored while typing in text fields)
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T))
            g_GizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            g_GizmoOperation = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_R))
            g_GizmoOperation = ImGuizmo::ROTATE;
    }

    // The scene renders into the central dock node area (PassthruCentralNode)
    if (ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
    {
        ImGuizmo::SetRect(centralNode->Pos.x, centralNode->Pos.y, centralNode->Size.x, centralNode->Size.y);

        auto camera = m_RenderViewUI->GetCamera();
        float4x4 view = camera->GetViewMatrix();
        float4x4 proj = camera->GetProjectionMatrix();

        auto* node = m_SelectedNode.get();
        float4x4 world = node->GetWorldTransform();

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            g_GizmoOperation,
            g_GizmoMode,
            glm::value_ptr(world));

        const bool usingNow = ImGuizmo::IsUsing();
        if (usingNow && !g_GizmoDragging)
            g_GizmoDragStartTransform = node->GetLocalTransform(); // drag start: snapshot for ESC-cancel

        if (usingNow && ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            // ESC cancels the drag: revert the node to the transform captured at drag start
            // and drop ImGuizmo's using state (otherwise it stays stuck while the button is held).
            node->SetLocalTransform(g_GizmoDragStartTransform);
            ImGuizmo::Enable(false);
            g_GizmoDragging = false;
        }
        else if (usingNow)
        {
            g_GizmoDragging = true;

            // Convert the edited world matrix back to the node's local transform
            float4x4 parentWorld = float4x4_I;
            if (const auto* parent = node->GetParent())
                parentWorld = parent->GetWorldTransform();

            node->SetLocalTransform(gfx::Transform{ glm::inverse(parentWorld) * world });
        }
        else
        {
            g_GizmoDragging = false;
        }
    }

    ImGuizmo::PopID();
}

void alm::fw::FrameworkUI::AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture)
{
    auto* device = GetDeviceManager()->GetDevice();

    auto it = std::ranges::find_if(m_TextureWindows, [&title](const UITextureWindow& elem)
        { return elem.title == title; });

    if (it == m_TextureWindows.end())
    {
        auto textureWindow = UITextureWindow{
            .title = title,
            .texture = texture };
        
        if (texture->GetDesc().dimension == rhi::TextureDimension::TextureCube)
        {
            textureWindow.arrayView = device->CreateTextureSampledView(texture.get(), rhi::TextureSubresourceSet{
                0, rhi::TextureSubresourceSet::AllMipLevels, 0, 6 }, rhi::Format::UNKNOWN, rhi::TextureDimension::Texture2DArray);
        }

        m_TextureWindows.push_back(textureWindow);
    }
    else
    {
        ImGui::SetWindowFocus(title.c_str());
    }
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

void alm::fw::FrameworkUI::ShowErrorModal(const std::string& message)
{
    m_PendingErrorModal = message;
}

void alm::fw::FrameworkUI::SetSelectedNode(const alm::weak<alm::gfx::SceneGraphNode>& node)
{
    m_SelectedNode = node;

    auto outlineRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::ObjectOutlineRenderStage>();
    if (outlineRS)
    {
        if (m_SelectedNode && m_SelectedNode->GetLeaf() && m_SelectedNode->GetLeaf()->GetType() == gfx::SceneGraphLeaf::Type::MeshInstance)
        {
            auto meshInstance = alm::checked_pointer_cast<gfx::MeshInstance>(m_SelectedNode->GetLeaf());
            outlineRS->SetSelectedObject(meshInstance);
        }
        else
        {
            outlineRS->SetSelectedObject(nullptr);
        }
    }
}

void alm::fw::FrameworkUI::SetGridVisible(bool b)
{
    auto gridRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::GridRenderStage>();
    if (gridRS)
    {
        bool visible = gridRS->IsVisible();
        if (visible != b)
        {
            gridRS->SetVisible(b);
        }
    }
}

bool alm::fw::FrameworkUI::IsGridVisible() const
{
    auto gridRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::GridRenderStage>();
    if (gridRS)
    {
        return gridRS->IsVisible();
    }
    return false;
}

void alm::fw::FrameworkUI::SwitchWireframeRender()
{
    bool wireframeActive = m_RenderViewUI->GetRenderGraph()->GetCurrentRenderMode() == "Wireframe";
    if (wireframeActive)
    {
        m_RenderViewUI->GetRenderGraph()->SetActiveRenderMode("Default");
    }
    else
    {
        m_RenderViewUI->GetRenderGraph()->SetActiveRenderMode("Wireframe");
    }
}

std::optional<std::pair<uint2, uint2>> alm::fw::FrameworkUI::GetCentralViewport() const
{
    auto r = GetCentralNodeRect();
    if (!r)
        return {};

    // With ViewportsEnable ImGui coordinates are OS-absolute (main viewport Pos = the OS window position).
    // Make the origin window-relative, which is what the engine expects.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float2 relOrigin = glm::max(float2(r->first) - float2(vp->Pos.x, vp->Pos.y), float2(0.f));

    // Clip the bottom against the status bar (it overlays the bottom strip of the work area)
    float2 size = float2{ r->second };
    if (m_ShowBottomBar)
    {
        const float statusBarTop = vp->Size.y - ImGui::GetFrameHeight(); // window-relative
        size.y = std::max(0.f, std::min(relOrigin.y + size.y, statusBarTop) - relOrigin.y);
    }

    if (size.x < 1.f || size.y < 1.f)
        return {};   // degenerate (minimized / fully covered): caller resets to full viewport

    return std::pair<uint2, uint2>{ uint2(relOrigin), uint2(size) };
}

void alm::fw::FrameworkUI::OpenMaterialPanel(alm::gfx::Material* material)
{
    m_SelectedMaterial = material;
    m_ShowMaterials = true;
    m_ScrollToSelectedMaterial = true;
}

void alm::fw::FrameworkUI::SetRenderStats(float fps, float cpuTime, float cpuIdleTime, float gpuTime)
{
    m_FPS = fps;
    m_CPUTime = cpuTime;
    m_CPUIdleTime = cpuIdleTime;
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
            if (ImGui::MenuItem("Settings", "Ctrl+S", m_ShowSettings))
                m_ShowSettings = !m_ShowSettings;
            if (ImGui::MenuItem("World Outliner", "Ctrl+W", m_ShowWorldOutliner))
            {
                if (!m_ShowWorldOutliner)
                {
                    OpenWorldOutliner();
                }
                else
                {
                    m_ShowWorldOutliner = false;
                }
            }

            if (ImGui::MenuItem("Render Stages", "Ctrl+R", m_ShowRenderStages))
                m_ShowRenderStages = !m_ShowRenderStages;

            if (ImGui::MenuItem("Material Panel", "Ctrl+M", m_ShowMaterials))
                m_ShowMaterials = !m_ShowMaterials;

            if (ImGui::MenuItem("Atmosphere Panel", nullptr, m_ShowAtmospherePanel))
                m_ShowAtmospherePanel = !m_ShowAtmospherePanel;

            if (ImGui::MenuItem("Statistics", nullptr, m_ShowStats))
                m_ShowStats = !m_ShowStats;

            if (ImGui::MenuItem("ImGui Debugger", nullptr, m_ShowImGuiDebugger))
                m_ShowImGuiDebugger = !m_ShowImGuiDebugger;

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Options"))
        {
            if (ImGui::BeginMenu("Grid"))
            {
                bool visible = IsGridVisible();
                auto gridRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::GridRenderStage>();
                if (gridRS)
                {
                    bool visible = gridRS->IsVisible();
                    if (ImGui::MenuItem("Visible", "Ctrl+G", visible))
                        gridRS->SetVisible(!visible);

                    float extent = gridRS->GetExtent();
                    if (ImGui::SliderFloat("Extent", &extent, 10.f, 1000.f, "%.0f", ImGuiSliderFlags_Logarithmic))
                        gridRS->SetExtent(extent);
                }
                ImGui::EndMenu();
            }

            bool wireframeActive = m_RenderViewUI->GetRenderGraph()->GetCurrentRenderMode() == "Wireframe";
            if (ImGui::MenuItem("Wireframe mode", "Ctrl+Alt+W", wireframeActive))
            {
                SwitchWireframeRender();
            }

            bool freezeCulling = m_RenderViewUI->GetFreezeCulling();
            if (ImGui::MenuItem("Freeze Culling", nullptr, freezeCulling))
            {
                m_RenderViewUI->SetFreezeCulling(!freezeCulling);
            }

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

        float cursorX = 0.f;
        const float cursorY = (statusBarHeight - textHeight) * 0.5f;

        ImGui::SetCursorPosX(cursorX);
        ImGui::SetCursorPosY(cursorY);

        ImGui::Text(" FPS: %1.1f", m_FPS);
        cursorX = 100.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| Frame time: %1.2f ms", m_CPUTime);
        cursorX += 180.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| CPU: %1.2f ms", m_CPUTime - m_CPUIdleTime);
        cursorX += 140.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| GPU: %1.2f ms", m_GPUTime);
        cursorX += 140.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| Draw calls: %d", renderStats.DrawCalls);
        cursorX += 140.f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("| Primitives: %d", renderStats.PrimitiveCount);

        // Right aligned user texts
        cursorX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        for (const std::string& text : m_BottomBarRightAlignTexts)
        {
            std::string actualText = std::format(" | {}", text);
            float textWidth = ImGui::CalcTextSize(actualText.c_str()).x;
            cursorX -= textWidth;
            ImGui::SameLine();
            ImGui::SetCursorPosX(cursorX);
            ImGui::TextUnformatted(actualText.c_str());
        }
        m_BottomBarRightAlignTexts.clear();
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void alm::fw::FrameworkUI::BuildTextureWindows()
{
    auto* device = GetDeviceManager()->GetDevice();

    auto it = m_TextureWindows.begin();
    while (it != m_TextureWindows.end())
    {
        if (!BuildTextureWindow(*it))
        {
            if (it->arrayView.IsValid())
                device->ReleaseTextureSampledView(it->arrayView);

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
    if (!tw.texture || tw.texture.expired())
        return false;

    bool isOpen = true;
    const auto& texDesc = tw.texture->GetDesc();
    ImVec2 imageSize{ tw.defaultImageWidth, tw.defaultImageWidth * texDesc.height / texDesc.width };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float detailsHeight = ImGui::GetTextLineHeightWithSpacing() * 6.0f + ImGui::GetFrameHeightWithSpacing();
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

        struct Prop
        {
            const char* label;
            bool isText;
            int intValue;
            const char* textValue;
        };
        constexpr int propertyCount = 7;
        const Prop properties[propertyCount] = {
            { "Width", false, (int)texDesc.width, "" },
            { "Height", false, (int)texDesc.height, "" },
            { "Depth", false, (int)texDesc.depth, "" },
            { "ArraySize", false, (int)texDesc.arraySize, "" },
            { "MipLevels", false, (int)texDesc.mipLevels, "" },
            { "Format", true, 0, alm::rhi::GetFormatString(texDesc.format) },
            { "Dimension", true, 0, GetTextureDimensionText(texDesc.dimension) },
        };

        const int propertyRows = (propertyCount + 1) / 2;
        if (ImGui::BeginTable("##properties", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##t0", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("##v0", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("##t1", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("##v1", ImGuiTableColumnFlags_WidthStretch, 1.2f);

            for (int row = 0; row < propertyRows; ++row)
            {
                ImGui::TableNextRow(0, ImGui::GetFrameHeight());
                for (int half = 0; half < 2; ++half)
                {
                    const int propIdx = row * 2 + half;

                    // Title cell
                    ImGui::TableSetColumnIndex(half * 2);
                    if (propIdx < propertyCount)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(properties[propIdx].label);
                    }

                    // Value cell
                    ImGui::TableSetColumnIndex(half * 2 + 1);
                    if (propIdx < propertyCount)
                    {
                        ImGui::PushID(++id);
                        ImGui::BeginDisabled();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (properties[propIdx].isText)
                        {
                            const char* text = properties[propIdx].textValue;
                            ImGui::InputText("##value", (char*)text, strlen(text), ImGuiInputTextFlags_ReadOnly);
                        }
                        else
                        {
                            int value = properties[propIdx].intValue;
                            ImGui::InputInt("##value", &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
                        }
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndTable();
        }

        fitPressed = ImGui::Button("Fit");
        ImGui::SameLine(0.f, 20.0f);
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

        ImGui::SameLine(0.f, 20.f);

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
            ImGui::SameLine(0.f, 20.0f);

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

        else if (texDesc.dimension == rhi::TextureDimension::Texture2DArray)
        {
            ImGui::SameLine(0.f, 20.0f);

            ImGui::SetNextItemWidth(60);
            std::vector<std::string> indices;
            indices.resize(texDesc.arraySize);
            for (int i = 0; i < texDesc.arraySize; ++i)
            {
                indices[i] = std::to_string(i);
            }
            if (ImGui::BeginCombo("Slice", indices[tw.selectedSlice].c_str()))
            {
                for (int i = 0; i < texDesc.arraySize; ++i)
                {
                    if (ImGui::Selectable(indices[i].c_str(), tw.selectedSlice == i))
                    {
                        tw.selectedSlice = i;
                    }
                }
                ImGui::EndCombo();
            }
        }

        else if (texDesc.dimension == rhi::TextureDimension::TextureCube)
        {
            ImGui::SameLine(0.f, 20.0f);

            ImGui::SetNextItemWidth(60);
            std::vector<std::string> slice;
            slice.resize(6);
            slice[0] = "+X";
            slice[1] = "-X";
            slice[2] = "+Y";
            slice[3] = "-Y";
            slice[4] = "+Z";
            slice[5] = "-Z";
            if (ImGui::BeginCombo("Face", slice[tw.selectedSlice].c_str()))
            {
                for (int i = 0; i < 6; ++i)
                {
                    if (ImGui::Selectable(slice[i].c_str(), tw.selectedSlice == i))
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

    ImGui::BeginChild("##image", ImVec2{ 0.f, 0.f }, ImGuiChildFlags_None, imageChildFlags);
    {
        // Visible area. NOT GetContentRegionAvail(): SetNextWindowContentSize above would make it
        // return the forced content size (the stale image size), not the actual visible hole.
        ImVec2 avail = ImGui::GetCurrentWindow()->ClipRect.GetSize();

        // Keep refitting while the layout settles right after the window opens (or after Fit):
        // ImGui decides scrollbar visibility with previous-frame window values, so the hole can
        // shrink one frame later (scrollbar reservations from the last frame). Without this, the
        // image stays sized for the old hole and both scrollbars appear for good.
        // After settling, only Fit refits: resizing the window keeps the image size (scrollable).
        if (fitPressed)
            tw.settleFrames = 2;
        if (tw.firstShow || fitPressed || tw.settleFrames > 0)
        {
            float scale = (std::max)((float)texDesc.width / avail.x, (float)texDesc.height / avail.y);
            tw.defaultImageWidth = texDesc.width / scale - 1.f;
            imageSize = { tw.defaultImageWidth, tw.defaultImageWidth * texDesc.height / texDesc.width };
        }

        rhi::TextureSampledView view = tw.texture->GetSampledView();
        if (tw.arrayView.IsValid())
            view = tw.arrayView;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ShowImage(tw.texture, view, float2{ imageSize.x, imageSize.y }, { 0.f , 0.f }, { 1.f, 1.f }, tw.selectedMip, tw.selectedSlice, texFlags);
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::End();

    tw.firstShow = false;
    if (tw.settleFrames > 0)
        --tw.settleFrames;
    return isOpen;
}

// Modal window to report errors to the user. Any code can queue a message by assigning
// m_PendingErrorModal; it shows up as a blocking ImGui modal until dismissed.
void alm::fw::FrameworkUI::ShowPendingErrorModal()
{
    if (m_PendingErrorModal.empty())
        return;

    if (!ImGui::IsPopupOpen("Error"))
        ImGui::OpenPopup("Error");

    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0f);
        ImGui::TextUnformatted(m_PendingErrorModal.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();

        const float buttonWidth = 120.f;
        const float offset = (ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f;
        if (offset > 0.f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        if (ImGui::Button("Ok", ImVec2(buttonWidth, 0.f)))
        {
            m_PendingErrorModal.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool alm::fw::FrameworkUI::BuildTextureShowBrowseClear(const char* id, std::shared_ptr<alm::gfx::LoadedTexture>& loadedTex,
    alm::gfx::TextureLoadFlags loadFlags)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;
    bool texUpdated = false;

    std::string title = "<empty>";
    if (loadedTex)
    {
        size_t pos = loadedTex->id.find_last_of('/');
        if (pos != std::string::npos)
        {
            title = loadedTex->id.substr(pos + 1);
        }
        else
        {
            title = loadedTex->id;
        }
    }

    ImGui::PushID(id);

    ImGui::SeparatorText(id);

    float buttonsWidth = ImGui::CalcItemWidth();
    float buttonWidth = (buttonsWidth / 3) - (style.ItemInnerSpacing.x * 1);

    ImGui::BeginDisabled(!loadedTex);
    if (ImGui::Button("Show", ImVec2(buttonWidth, 0.f)))
    {
        AddTextureWindow(title, loadedTex->texture.get_weak());
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(buttonWidth, 0.f)))
    {
        std::string path = OpenFileNativeDialog(loadedTex ? loadedTex->id : std::string{}, {
            { "Image files", "*.png;*.tga;*.jpg;*.jpeg;*.dds;*.ktx2" },
            { "TGA", "*.tga" },
            { "PNG", "*.png" },
            { "JPEG", "*.jpg;*.jpeg" },
            { "DDS", "*.dds" },
            { "KTX2", "*.ktx2" }});
        if (!path.empty())
        {
            auto loadResult = GetDeviceManager()->GetTextureCache()->Load(path, loadFlags);
            if (loadResult)
            {
                loadResult->second.Wait();
                loadedTex = loadResult->first;
                texUpdated = true;
            }
            else
            {
                ShowErrorModal(std::string(
                    "Failed to load texture:\n\"") + path + "\"\n\n" + loadResult.error());
            }
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!loadedTex);
    if (ImGui::Button("Clear", ImVec2(buttonWidth, 0.f)))
    {
        loadedTex.reset();
        texUpdated = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::TextUnformatted(title.c_str());

    ImGui::PopID();
    return texUpdated;
}

bool alm::fw::FrameworkUI::BuildTextureShowBrowseClearPred(const char* id, std::shared_ptr<alm::gfx::LoadedTexture>& loadedTex,
    alm::gfx::TextureLoadFlags loadFlags, std::function<bool(const alm::gfx::LoadedTexture&)> pred)
{
    auto newTexture = loadedTex;
    if (BuildTextureShowBrowseClear(id, newTexture, alm::gfx::TextureLoadFlags::None))
    {
        if (!newTexture || pred(*newTexture))
        {
            loadedTex = newTexture;
            return true;
        }
    }    
    return false;
}

std::string alm::fw::FrameworkUI::OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters)
{
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(m_Window);
    HWND hWnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[260] = { 0 };
    if (!filename.empty())
    {
        std::filesystem::path p(filename);
        strncpy_s(szFile, p.filename().string().c_str(), _TRUNCATE);
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
        for (const auto& p : filters)
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

    std::string initialDir;
    if (!filename.empty())
    {
        std::error_code ec;
        std::filesystem::path p(filename);

        std::filesystem::path abs = std::filesystem::absolute(p, ec);
        if (!ec)
        {
            std::filesystem::path dir = abs.parent_path();
            if (std::filesystem::is_directory(dir, ec))
                initialDir = dir.string();
        }
    }
    ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

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
