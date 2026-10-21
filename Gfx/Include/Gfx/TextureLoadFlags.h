#pragma once

#include "Core/Common.h"

namespace alm::gfx
{
	enum class TextureLoadFlags
	{
		None = 0,
		ForceSRGB = 1 << 0,			// SRGB format
		GenerateMips = 1 << 1,	// Generate mips if not present in the data
		IsNormalMap = 1 << 2	// When generate mips, renormalize
	};

	ALM_ENUM_CLASS_FLAG_OPERATORS(TextureLoadFlags)
} // namespace alm::gfx