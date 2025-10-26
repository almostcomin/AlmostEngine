#pragma once

namespace st::rapi
{

enum class ResourceState
{
	// Common resource states:
	UNDEFINED = 0,						// invalid state (don't preserve contents)
	SHADER_RESOURCE = 1 << 0,			// shader resource, read only
	SHADER_RESOURCE_COMPUTE = 1 << 1,	// shader resource, read only, non-pixel shader
	UNORDERED_ACCESS = 1 << 2,			// shader resource, write enabled
	COPY_SRC = 1 << 3,					// copy from
	COPY_DST = 1 << 4,					// copy to

	// Texture specific resource states:
	RENDERTARGET = 1 << 5,				// render target, write enabled
	DEPTHSTENCIL = 1 << 6,				// depth stencil, write enabled
	DEPTHSTENCIL_READONLY = 1 << 7,		// depth stencil, read only
	SHADING_RATE_SOURCE = 1 << 8,		// shading rate control per tile

	// GPUBuffer specific resource states:
	VERTEX_BUFFER = 1 << 9,				// vertex buffer, read only
	INDEX_BUFFER = 1 << 10,				// index buffer, read only
	CONSTANT_BUFFER = 1 << 11,			// constant buffer, read only
	INDIRECT_ARGUMENT = 1 << 12,		// argument buffer to DrawIndirect() or DispatchIndirect()
	RAYTRACING_ACCELERATION_STRUCTURE = 1 << 13, // acceleration structure storage or scratch
	PREDICATION = 1 << 14,				// storage for predication comparison value

	// Other:
	VIDEO_DECODE_SRC = 1 << 15,			// video decode operation source (bitstream buffer or DPB texture)
	VIDEO_DECODE_DST = 1 << 16,			// video decode operation destination output texture
	VIDEO_DECODE_DPB = 1 << 17,			// video decode operation destination DPB texture
	SWAPCHAIN = 1 << 18,				// resource state of swap chain's back buffer texture when it's not rendering
};

ENUM_CLASS_FLAG_OPERATORS(ResourceState)

} // namespace st::rapi