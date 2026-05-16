#pragma once

namespace alm::gfx
{
	struct GpuSceneBuffersHandle
	{
		friend class GpuSceneBuffers;
	private:
		uint32_t idx = UINT32_MAX;
	};
}