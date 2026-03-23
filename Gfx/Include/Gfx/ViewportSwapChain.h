#pragma once

struct SDL_Window;

#include "RHI/Format.h"

namespace alm::gfx
{
	using ViewportSwapChainId = void*;

	struct ViewportSwapChainInitParams
	{
		SDL_Window* WindowHandle = nullptr;
		alm::rhi::Format Format = rhi::Format::RGBA8_UNORM;
	};
}

