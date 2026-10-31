#pragma once

// IBL baking adapted from Google Filament libs/ibl (Apache License 2.0).
// Copyright (C) 2015 The Android Open Source Project
// https://github.com/google/filament/tree/main/libs/ibl
// Ported to AlmostEngine: glm math, no JobSystem dependency, CPU multi-threaded.

#include "Core/Math.h"

#include <array>
#include <expected>
#include <string>
#include <vector>

namespace alm::gfx
{

struct IBLBakeParams
{
	uint32_t SkySize = 1024;               // base face size of the skybox mip chain (POT)
	uint32_t LambertSize = 256;            // face size of the diffuse irradiance cubemap (POT)
	uint32_t GgxSize = 256;                // base face size of the prefiltered specular cubemap (POT)
	uint32_t GgxMinLodSize = 2;            // face size of the last GGX mip (POT, <= GgxSize)
	uint32_t GgxSampleCount = 1024;        // GGX prefilter samples per texel
	uint32_t IrradianceSampleCount = 1024; // diffuse irradiance samples per texel
	uint32_t DfgLutSize = 128;             // DFG LUT resolution
	bool MirrorX = true;                   // horizontal mirror of the source (cmgen default)
	bool KarisClamp = false;               // Karis tone compression before filtering (cmgen --clamp)
	bool DfgMultiscatter = false;          // false: split-sum DFG; true: multi-scattering DFG
	bool DfgCharlie = true;                // add Charlie (sheen) visibility as 3rd DFG channel
	uint32_t ShBands = 3;                  // SH decomposition bands (0-3), cmgen --sh-irradiance
};

// One mip level of a cubemap. Faces are row-major, FaceSize x FaceSize, no padding.
// Face order: +X, -X, +Y, -Y, +Z, -Z (same as cmgen / KTX2 cubemaps).
struct IBLCubemapLevel
{
	uint32_t FaceSize = 0;
	std::array<std::vector<float3>, 6> Faces;
};

struct IBLDFGLut
{
	uint32_t Size = 0;
	bool HasCharlie = false;
	// Row-major Size x Size. X,Y = GGX DFG (split-sum or multi-scatter); Z = Charlie visibility.
	// Row 0 is roughness 1, row Size-1 is roughness 0 (U = NoV).
	std::vector<float3> Texels;
};

struct IBLBakeOutput
{
	std::vector<IBLCubemapLevel> Sky;     // full mip chain, base = SkySize
	std::vector<IBLCubemapLevel> Ggx;     // prefiltered chain, mip i -> perceptual roughness i/(count-1)
	std::vector<IBLCubemapLevel> Lambert; // single mip
	IBLDFGLut Dfg;
	std::array<float3, 9> Sh = {};        // windowed irradiance SH, 3 bands (unused bands are zero)
};

class IBLBaker
{
public:
	// pixels: RGBA float32 (stbi_loadf layout). Width must be 2*height (equirectangular).
	// Source values are clamped to the half-float range (65504) before filtering.
	// Runs multi-threaded; transient memory peaks around 3x the sky mip chain (cross layout
	// work buffers + exported data), so expect several hundred MB with the default SkySize.
	static std::expected<IBLBakeOutput, std::string> Bake(
		const float* rgba, uint32_t width, uint32_t height, const IBLBakeParams& params = {});

	// Pre-multiplies SH coefficients by the polynomial-form factors (and 1/pi), so a shader can
	// reconstruct irradiance without extra math. Matches Filament's CubemapSH::preprocessSHForShader.
	static void PreprocessSHForShader(std::array<float3, 9>& sh);
};

} // namespace alm::gfx
