#pragma once

#include <cstdint>

namespace st::rapi
{

enum class ComparisonFunc : uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum class StencilOp : uint8_t
{
	Keep,
	Zero,
	Replace,
	IncrementAndClamp,
	DecrementAndClamp,
	Invert,
	IncrementAndWrap,
	DecrementAndWrap
};

struct DepthStencilState
{
	bool depthTestEnable = false;
	bool depthWriteEnable = false;
	ComparisonFunc depthFunc = ComparisonFunc::Never;
	bool stencilEnable = false;
	uint8_t stencilReadMask = 0xff;
	uint8_t stencilWriteMask = 0xff;
	uint8_t stencilRefValue = 0;

	struct DepthStencilOp
	{
		StencilOp failOp = StencilOp::Keep;
		StencilOp depthFailOp = StencilOp::Keep;
		StencilOp passOp = StencilOp::Keep;
		ComparisonFunc func = ComparisonFunc::Always;
	};
	DepthStencilOp frontFaceStencil;
	DepthStencilOp backFaceStencil;
};

} // namespace rt::rapi