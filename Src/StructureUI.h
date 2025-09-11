#pragma once

#include "UI/ImGuiRenderPass.h"

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
};