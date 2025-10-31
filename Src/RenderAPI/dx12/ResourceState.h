#pragma once

#include <d3d12.h>
#include "RenderAPI/ResourceState.h"

namespace st::rapi::dx12
{
	constexpr D3D12_RESOURCE_STATES MapResourceState(ResourceState value)
	{
		D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

		if (has_flag(value, ResourceState::SHADER_RESOURCE))
			ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (has_flag(value, ResourceState::SHADER_RESOURCE_COMPUTE))
			ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (has_flag(value, ResourceState::UNORDERED_ACCESS))
			ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (has_flag(value, ResourceState::COPY_SRC))
			ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (has_flag(value, ResourceState::COPY_DST))
			ret |= D3D12_RESOURCE_STATE_COPY_DEST;

		if (has_flag(value, ResourceState::RENDERTARGET))
			ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (has_flag(value, ResourceState::DEPTHSTENCIL))
			ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (has_flag(value, ResourceState::DEPTHSTENCIL_READONLY))
			ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (has_flag(value, ResourceState::SHADING_RATE_SOURCE))
			ret |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;

		if (has_flag(value, ResourceState::VERTEX_BUFFER))
			ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (has_flag(value, ResourceState::INDEX_BUFFER))
			ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (has_flag(value, ResourceState::CONSTANT_BUFFER))
			ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (has_flag(value, ResourceState::INDIRECT_ARGUMENT))
			ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (has_flag(value, ResourceState::RAYTRACING_ACCELERATION_STRUCTURE))
			ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		if (has_flag(value, ResourceState::PREDICATION))
			ret |= D3D12_RESOURCE_STATE_PREDICATION;

		if (has_flag(value, ResourceState::VIDEO_DECODE_SRC))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
		if (has_flag(value, ResourceState::VIDEO_DECODE_DST))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
		if (has_flag(value, ResourceState::VIDEO_DECODE_DPB))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;

		if (has_flag(value, ResourceState::SWAPCHAIN))
			ret |= D3D12_RESOURCE_STATE_PRESENT;

		return ret;
	}

} // namespace st::rapi