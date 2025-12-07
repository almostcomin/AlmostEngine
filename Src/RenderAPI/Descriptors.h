#pragma once

#include <cstdint>

namespace st::rapi
{

using DescriptorIndex = uint32_t;
constexpr DescriptorIndex c_InvalidDescriptorIndex = ~0u;
/*
enum class DescriptorType : uint8_t
{
	CBV,
	SRV,
	UAV,
	RTV,
	DSV,

	_Size
};
*/

enum class TextureShaderView
{
	ShaderResource = 0,
	UnorderedAcces = 1,
	RenderTarget = 2,
	DepthStencil = 3,

	_Size
};
enum class TextureShaderUsage
{
	None = 0x0,
	ShaderResource = 0x1 << (int)TextureShaderView::ShaderResource,
	UnorderedAccess = 0x1 << (int)TextureShaderView::UnorderedAcces,
	RenderTarget = 0x1 << (int)TextureShaderView::RenderTarget,
	DepthStencil = 0x1 << (int)TextureShaderView::DepthStencil
};
ENUM_CLASS_FLAG_OPERATORS(TextureShaderUsage)

enum class BufferShaderView
{
	ConstantBuffer = 0,
	ShaderResource = 1,
	UnorderedAccess = 2,

	_Size
};
enum class BufferShaderUsage
{
	None = 0x0,
	ConstantBuffer = 0x1 << (int)BufferShaderView::ConstantBuffer,
	ShaderResource = 0x1 << (int)BufferShaderView::ShaderResource,
	UnorderedAccess = 0x1 << (int)BufferShaderView::UnorderedAccess
};
ENUM_CLASS_FLAG_OPERATORS(BufferShaderUsage)

enum class MemoryAccess
{
	Default,
	Upload,		// CPU write, GPU read
	Readback	// CPU read, GPU write
};

} // namespace st::rapi