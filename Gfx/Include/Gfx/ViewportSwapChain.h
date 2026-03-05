#pragma once

struct SDL_Window;

#include "RHI/Format.h"

namespace st::gfx
{
	using ViewportSwapChainId = void*;

	struct ViewportSwapChainInitParams
	{
		SDL_Window* WindowHandle = nullptr;
		st::rhi::Format Format = rhi::Format::RGBA8_UNORM;
	};
}

