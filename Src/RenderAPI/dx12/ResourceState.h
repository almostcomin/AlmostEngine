#pragma once

#include "RenderAPI/dx12/d3d12_headers.h"
#include "RenderAPI/ResourceState.h"

namespace st::rapi::dx12
{
	constexpr D3D12_RESOURCE_STATES MapResourceState(ResourceState value)
	{
		D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

		if (hasFlag(value, ResourceState::SHADER_RESOURCE))
			ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (hasFlag(value, ResourceState::SHADER_RESOURCE_COMPUTE))
			ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (hasFlag(value, ResourceState::UNORDERED_ACCESS))
			ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (hasFlag(value, ResourceState::COPY_SRC))
			ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (hasFlag(value, ResourceState::COPY_DST))
			ret |= D3D12_RESOURCE_STATE_COPY_DEST;

		if (hasFlag(value, ResourceState::RENDERTARGET))
			ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (hasFlag(value, ResourceState::DEPTHSTENCIL))
			ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (hasFlag(value, ResourceState::DEPTHSTENCIL_READONLY))
			ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (hasFlag(value, ResourceState::SHADING_RATE_SOURCE))
			ret |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;

		if (hasFlag(value, ResourceState::VERTEX_BUFFER))
			ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (hasFlag(value, ResourceState::INDEX_BUFFER))
			ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (hasFlag(value, ResourceState::CONSTANT_BUFFER))
			ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (hasFlag(value, ResourceState::INDIRECT_ARGUMENT))
			ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (hasFlag(value, ResourceState::RAYTRACING_ACCELERATION_STRUCTURE))
			ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		if (hasFlag(value, ResourceState::PREDICATION))
			ret |= D3D12_RESOURCE_STATE_PREDICATION;

		if (hasFlag(value, ResourceState::VIDEO_DECODE_SRC))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
		if (hasFlag(value, ResourceState::VIDEO_DECODE_DST))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
		if (hasFlag(value, ResourceState::VIDEO_DECODE_DPB))
			ret |= D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;

		if (hasFlag(value, ResourceState::SWAPCHAIN))
			ret |= D3D12_RESOURCE_STATE_PRESENT;

		return ret;
	}

} // namespace st::rapi