#pragma once

namespace alm::gfx
{
	enum class LightingMode : uint8_t
	{
		Atmosphere,
		Environment // IBL
	};

	// Which sky/background source renders behind the scene.
	// Constrained by LightingMode:
	// - Atmosphere mode only supports Simple/Atmos
	// - Environment mode only supports Cubemap
	enum class SkySource : uint8_t
	{
		Simple = 0,
		Atmos = 1,
		Cubemap = 2
	};
} // namespace alm::gfx