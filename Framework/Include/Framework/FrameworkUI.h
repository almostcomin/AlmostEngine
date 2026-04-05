#pragma once

#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::fw
{

class FrameworkUI : public alm::gfx::ImGuiRenderStage
{
	REGISTER_RENDER_STAGE(FrameworkUI)

public:

	void ShowBottomBar(bool b) { m_ShowBottomBar = b; }
	void AddTextureWindow(const std::string& title, alm::rhi::TextureHandle texture);

	void SetRenderStats(float fps, float cpuTime, float gpuTime);

protected:

	void BuildUI() override;
	
	static bool ShowToggleButton(const char* label, bool* v);
	static void ShowPropertyInt(const char* label, float labelWidth, int value, float valueWidth, int id);
	static void ShowPropertyText(const char* label, float labelWidth, const char* value, float valueWidth, int id);

private:

	struct UITextureWindow
	{
		std::string title;
		alm::rhi::TextureHandle texture;
		bool opaque = false;
		bool redChannel = true;
		bool greenChannel = true;
		bool blueChannel = true;
		bool alphaChannel = false;
		float defaultImageWidth = 360.f;
		bool firstShow = true;
		int selectedMip = 0;
		int selectedSlice = 0;
	};

private:

	void BuildBottomBar();
	void BuildTextureWindows();
	bool BuildTextureWindow(UITextureWindow& textureWindow);

private:

	float m_FPS = 0.f;
	float m_CPUTime = 0.f;
	float m_GPUTime = 0.f;
	bool m_ShowBottomBar = true;

	std::vector<UITextureWindow> m_TextureWindows;
};

} // namespace alm::fw