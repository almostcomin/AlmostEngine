#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include <functional>

class StructureUI : public st::gfx::ImGuiRenderStage
{
public:

	struct Data
	{
		bool ShowUI = true;
		bool ShowFPS = true;
		float FPS = 0.f;
	};

	StructureUI(SDL_Window* window);

	void BuildUI() override;

public:

	Data m_Data;

	std::function<void(const char*)>  m_RequestLoadFile;

private:

	void BuildMainMenu();
	void BuildMenuFile();

	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

private:

	SDL_Window* m_Window;
};