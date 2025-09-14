#pragma once

#include "UI/ImGuiRenderPass.h"
#include <functional>

class StructureUI : public st::ui::ImGuiRenderPass
{
public:

	struct Data
	{
		bool ShowUI = true;
		bool ShowFPS = true;
		float FPS = 0.f;
	};

	bool Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory);

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