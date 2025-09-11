#include "StructureUI.h"
#include <format>

bool StructureUI::Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory)
{
	return st::ui::ImGuiRenderPass::Init(window, deviceManager, shaderFactory);
}

void StructureUI::BuildUI()
{
    if (!m_Data.ShowUI)
        return;

    ImGuiIO const& io = ImGui::GetIO();

    BeginFullScreenWindow();
    DrawCenteredText("Hello world!");

    // Show FPS
    {
        std::string fps = std::format(" {:.3f} FPS", m_Data.FPS);

        ImVec2 textSize = ImGui::CalcTextSize(fps.c_str());
        ImGui::SetCursorPosX(0.f);
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - textSize.y);
        ImGui::TextUnformatted(fps.c_str());
    }

    EndFullScreenWindow();
}