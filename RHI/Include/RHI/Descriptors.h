#pragma once

#include <cstdint>
#include "Core/Common.h"

namespace st::rhi
{

using DescriptorIndex = uint32_t;
constexpr DescriptorIndex c_InvalidDescriptorIndex = ~0u; // Same as INVALID_DESCRIPTOR_INDEX

enum class TextureViewType
{
	Sampled = 0,
	Storage = 1,
	ColorTarget = 2,
	DepthTarget = 3,

	_Size
};

enum class BufferViewType
{
	Uniform = 0,
	ReadOnly = 1,
	ReadWrite = 2,

	_Size
};

enum class TextureShaderUsage
{
	None = 0x0,
	Sampled = 0x1 << (int)TextureViewType::Sampled,
	Storage = 0x1 << (int)TextureViewType::Storage,
	ColorTarget = 0x1 << (int)TextureViewType::ColorTarget,
	DepthTarget = 0x1 << (int)TextureViewType::DepthTarget
};
ENUM_CLASS_FLAG_OPERATORS(TextureShaderUsage)

enum class BufferShaderUsage
{
	None = 0x0,
	Uniform = 0x1 << (int)BufferViewType::Uniform,
	ReadOnly = 0x1 << (int)BufferViewType::ReadOnly,
	ReadWrite = 0x1 << (int)BufferViewType::ReadWrite
};
ENUM_CLASS_FLAG_OPERATORS(BufferShaderUsage)

enum class MemoryAccess
{
	Default,
	Upload,		// CPU write, GPU read
	Readback	// CPU read, GPU write
};

} // namespace st::rhi