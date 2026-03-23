#pragma once

#include <cstdint>

namespace alm::rhi
{

enum class BlendFactor : uint8_t
{
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DstAlpha,
	InvDstAlpha,
	DstColor,
	InvDstColor,
	SrcAlphaSaturate,
	ConstantColor,
	InvConstantColor,
	Src1Color,
	InvSrc1Color,
	Src1Alpha,
	InvSrc1Alpha,

	// Vulkan names
	OneMinusSrcColor = InvSrcColor,
	OneMinusSrcAlpha = InvSrcAlpha,
	OneMinusDstAlpha = InvDstAlpha,
	OneMinusDstColor = InvDstColor,
	OneMinusConstantColor = InvConstantColor,
	OneMinusSrc1Color = InvSrc1Color,
	OneMinusSrc1Alpha = InvSrc1Alpha
};

enum class BlendOp : uint8_t
{
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max
};

enum class ColorMask : uint8_t
{
	// These values are equal to their counterparts in DX11, DX12, and Vulkan.
	Red = 1,
	Green = 2,
	Blue = 4,
	Alpha = 8,
	All = 0xF
};

struct BlendState
{
	bool alphaToCoverageEnable = false;
	bool independentBlendEnable = true;

	struct RenderTargetBlendState
	{
		bool blendEnable = false;
		BlendFactor srcBlend = BlendFactor::SrcAlpha;
		BlendFactor destBlend = BlendFactor::OneMinusSrcAlpha;
		BlendOp blendOp = BlendOp::Add;
		BlendFactor srcBlendAlpha = BlendFactor::One;
		BlendFactor destBlendAlpha = BlendFactor::Zero;
		BlendOp blendOpAlpha = BlendOp::Add;
		ColorMask renderTargetWriteMask = ColorMask::All;
	};
	RenderTargetBlendState renderTarget[8];
};

} // namespace st::rhi