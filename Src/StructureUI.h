#pragma once

#include "UI/ImGuiRenderPass.h"

class StructureUI : public st::ui::ImGuiRenderPass
{
public:

	struct Data
	{
		bool ShowUI = true;
	};

	bool Init(SDL_Window* window, nvrhi::DeviceHandle device, st::gfx::ShaderFactory* shaderFactory);

	void Build() override;

public:

	Data mm_Data;
};