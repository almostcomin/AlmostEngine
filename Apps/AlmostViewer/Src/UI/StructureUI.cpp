#include "StructureUI.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"

StructureUI::StructureUI()
{}

StructureUI::~StructureUI()
{}

void StructureUI::BuildUI()
{
    FrameworkUI::BuildUI();

    ImGuiIO const& io = ImGui::GetIO();

    BeginFullScreenWindow();
    {
        if (GetScene()->GetSceneGraph()->GetRoot()->GetChildrenCount() == 0)
        {
            DrawCenteredText("Click File->Open to open a scene file");
        }
    }
    EndFullScreenWindow();
}

void StructureUI::OnAttached()
{
    alm::gfx::ImGuiRenderStage::OnAttached();
}
