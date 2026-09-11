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

    BeginFullScreenWindow();
    {
        auto root = GetScene()->GetSceneGraph()->GetRoot();
        if(!root || root->GetChildrenCount() == 0)
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
