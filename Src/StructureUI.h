#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include <functional>

namespace st::gfx
{
	class SceneGraphNode;
	class MeshInstance;
};

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

	std::function<void(const char*)> m_RequestLoadFile;
	std::function<void()> m_RequestClose;
	std::function<void()> m_RequestQuit;

private:

	void BuildMainMenu();
	void BuildMenuFile();
	void BuildSceneWindow(bool* p_open);
	void BuildMeshInstanceLeaf(const st::gfx::MeshInstance* leaf);

	std::string OpenFileNativeDialog(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& filters);
	std::string SaveFileNativeDialog(const std::string& filename);

private:

	SDL_Window* m_Window;

	bool m_ShowSceneWindow;
	const st::gfx::SceneGraphNode* m_SelectedNode;
};