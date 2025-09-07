#include "StructureUI.h"

bool StructureUI::Init(SDL_Window* window, nvrhi::DeviceHandle device, st::gfx::ShaderFactory* shaderFactory)
{
	return st::ui::ImGuiRenderPass::Init(window, device, shaderFactory);
}

void StructureUI::Build()
{
    if (!mm_Data.ShowUI)
        return;
}