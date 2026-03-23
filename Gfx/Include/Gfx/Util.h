#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dxgi.h>

#include "Core/Math.h"
#include "RHI/Format.h"

namespace alm::gfx
{
	rhi::Format GetFormat(DXGI_FORMAT format);
	uint32_t BitsPerPixel(rhi::Format format);

	// Perspective matrix, right handed, column major, inverse z
	float4x4 BuildPersInvZ(float v_fov, float aspect, float z_near, float z_far);
	// Perspective matrix, right handed, column major, inverse z, infinite far
	float4x4 BuildPersInvZInfFar(float v_fov, float aspect, float z_near);
	// Orthographic matrix, right handed, column major, inverse z, D3D/Vulkan NDC [1..0]
	float4x4 BuildOrthoInvZ(float left, float right, float bottom, float top, float z_near, float z_far);

} // namespace st::gfx