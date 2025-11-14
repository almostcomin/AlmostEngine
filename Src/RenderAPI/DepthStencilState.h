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
	GreaterOrEqual,
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
		StencilOp stencil_fail_op = StencilOp::Keep;
		StencilOp stencil_depth_fail_op = StencilOp::Keep;
		StencilOp stencil_pass_op = StencilOp::Keep;
		ComparisonFunc stencil_func = ComparisonFunc::Always;
	};
	DepthStencilOp frontFaceStencil;
	DepthStencilOp backFaceStencil;
};

} // namespace rt::rapi