#include "RenderAPI/dx12/Utils.h"
#include "RenderAPI/dx12/DxgiFormat.h"
#include <cassert>

namespace
{
	D3D12_BLEND GetBlend(st::rapi::BlendFactor factor)
	{
		switch (factor)
		{
		case st::rapi::BlendFactor::Zero: return D3D12_BLEND_ZERO;
		case st::rapi::BlendFactor::One: return D3D12_BLEND_ONE;
		case st::rapi::BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
		case st::rapi::BlendFactor::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
		case st::rapi::BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
		case st::rapi::BlendFactor::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
		case st::rapi::BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
		case st::rapi::BlendFactor::InvDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
		case st::rapi::BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
		case st::rapi::BlendFactor::InvDstColor: return D3D12_BLEND_INV_DEST_COLOR;
		case st::rapi::BlendFactor::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
		case st::rapi::BlendFactor::ConstantColor: return D3D12_BLEND_BLEND_FACTOR;
		case st::rapi::BlendFactor::InvConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
		case st::rapi::BlendFactor::Src1Color: return D3D12_BLEND_SRC1_COLOR;
		case st::rapi::BlendFactor::InvSrc1Color: return D3D12_BLEND_INV_SRC1_COLOR;
		case st::rapi::BlendFactor::Src1Alpha: return D3D12_BLEND_SRC1_ALPHA;
		case st::rapi::BlendFactor::InvSrc1Alpha: return D3D12_BLEND_INV_SRC1_ALPHA;
		default:
			assert(0);
			return D3D12_BLEND_ZERO;
		};
	}

	D3D12_BLEND_OP GetBlendOp(st::rapi::BlendOp op)
	{
		switch (op)
		{
		case st::rapi::BlendOp::Add: return D3D12_BLEND_OP_ADD;
		case st::rapi::BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
		case st::rapi::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
		case st::rapi::BlendOp::Min: return D3D12_BLEND_OP_MIN;
		case st::rapi::BlendOp::Max: return D3D12_BLEND_OP_MAX;
		default:
			assert(0);
			return D3D12_BLEND_OP_ADD;
		}
	}

	D3D12_FILL_MODE GetFillMode(st::rapi::FillMode mode)
	{
		switch (mode)
		{
		case st::rapi::FillMode::Solid: return D3D12_FILL_MODE_SOLID;
		case st::rapi::FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		default:
			assert(0);
			return D3D12_FILL_MODE_SOLID;
		}
	}

	D3D12_CULL_MODE GetCullMode(st::rapi::CullMode mode)
	{
		switch (mode)
		{
		case st::rapi::CullMode::Back: return D3D12_CULL_MODE_BACK;
		case st::rapi::CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case st::rapi::CullMode::None: return D3D12_CULL_MODE_NONE;
		default:
			assert(0);
			return D3D12_CULL_MODE_BACK;
		}
	}

	D3D12_COMPARISON_FUNC GetComparisonFunc(st::rapi::ComparisonFunc func)
	{
		switch (func)
		{
		case st::rapi::ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case st::rapi::ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
		case st::rapi::ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case st::rapi::ComparisonFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case st::rapi::ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case st::rapi::ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case st::rapi::ComparisonFunc::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case st::rapi::ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		default:
			assert(0);
			return D3D12_COMPARISON_FUNC_NEVER;
		}
	}

	D3D12_STENCIL_OP GetStencilOp(st::rapi::StencilOp op)
	{
		switch (op)
		{
		case st::rapi::StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
		case st::rapi::StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
		case st::rapi::StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
		case st::rapi::StencilOp::IncrementAndClamp: return D3D12_STENCIL_OP_INCR_SAT;
		case st::rapi::StencilOp::DecrementAndClamp: return D3D12_STENCIL_OP_DECR_SAT;
		case st::rapi::StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
		case st::rapi::StencilOp::IncrementAndWrap: return D3D12_STENCIL_OP_INCR;
		case st::rapi::StencilOp::DecrementAndWrap: return D3D12_STENCIL_OP_DECR;
		default:
			assert(0);
			return D3D12_STENCIL_OP_KEEP;
		}
	}

} // anonymous namespace

D3D12_BLEND_DESC st::rapi::dx12::GetBlendDesc(const st::rapi::BlendState& desc)
{
	D3D12_BLEND_DESC ret = {};
	
	ret.AlphaToCoverageEnable = desc.alphaToCoverageEnable;
	ret.IndependentBlendEnable = desc.independentBlendEnable;
	for (int i = 0; i < 8; ++i)
	{
		ret.RenderTarget[i].BlendEnable = desc.renderTarget[i].blendEnable;
		ret.RenderTarget[i].LogicOpEnable = false;
		ret.RenderTarget[i].SrcBlend = GetBlend(desc.renderTarget[i].srcBlend);
		ret.RenderTarget[i].DestBlend = GetBlend(desc.renderTarget[i].destBlend);
		ret.RenderTarget[i].BlendOp = GetBlendOp(desc.renderTarget[i].blendOp);
		ret.RenderTarget[i].SrcBlendAlpha = GetBlend(desc.renderTarget[i].srcBlendAlpha);
		ret.RenderTarget[i].DestBlendAlpha = GetBlend(desc.renderTarget[i].destBlendAlpha);
		ret.RenderTarget[i].BlendOpAlpha = GetBlendOp(desc.renderTarget[i].blendOpAlpha);
		ret.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
		ret.RenderTarget[i].RenderTargetWriteMask = (UINT8)desc.renderTarget[i].renderTargetWriteMask;
	}

	return ret;
}

D3D12_RASTERIZER_DESC st::rapi::dx12::GetRasterizerState(const st::rapi::RasterizerState& rasterState)
{
	D3D12_RASTERIZER_DESC ret = {};

	ret.FillMode = GetFillMode(rasterState.fillMode);
	ret.CullMode = GetCullMode(rasterState.cullMode);
	ret.FrontCounterClockwise = rasterState.frontCounterClockwise;
	ret.DepthBias = rasterState.depthBias;
	ret.DepthBiasClamp = rasterState.depthBiasClamp;
	ret.SlopeScaledDepthBias = rasterState.slopeScaledDepthBias;
	ret.DepthClipEnable = rasterState.depthClipEnable;
	ret.MultisampleEnable = rasterState.multisampleEnable;
	ret.AntialiasedLineEnable = rasterState.antialiasedLineEnable;
	ret.ForcedSampleCount = rasterState.forcedSampleCount;
	ret.ConservativeRaster = rasterState.conservativeRasterEnable ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return ret;
}

D3D12_DEPTH_STENCIL_DESC st::rapi::dx12::GetDepthStencilState(const st::rapi::DepthStencilState& depthStencilState)
{
	D3D12_DEPTH_STENCIL_DESC ret = {};

	ret.DepthEnable = depthStencilState.depthTestEnable;
	ret.DepthWriteMask = depthStencilState.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	ret.DepthFunc = GetComparisonFunc(depthStencilState.depthFunc);
	ret.StencilEnable = depthStencilState.stencilEnable;
	ret.StencilReadMask = depthStencilState.stencilReadMask;
	ret.StencilWriteMask = depthStencilState.stencilWriteMask;
	ret.FrontFace.StencilFailOp = GetStencilOp(depthStencilState.frontFaceStencil.failOp);
	ret.FrontFace.StencilDepthFailOp = GetStencilOp(depthStencilState.frontFaceStencil.depthFailOp);
	ret.FrontFace.StencilPassOp = GetStencilOp(depthStencilState.frontFaceStencil.passOp);
	ret.FrontFace.StencilFunc = GetComparisonFunc(depthStencilState.frontFaceStencil.func);

	return ret;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE st::rapi::dx12::GetPrimitiveType(st::rapi::PrimitiveTopology primTopo)
{
	switch (primTopo)
	{
	case st::rapi::PrimitiveTopology::PointList: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	case st::rapi::PrimitiveTopology::LineList:
	case st::rapi::PrimitiveTopology::LineStrip: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	case st::rapi::PrimitiveTopology::TriangleList:
	case st::rapi::PrimitiveTopology::TriangleStrip:
	case st::rapi::PrimitiveTopology::TriangleFan:
	case st::rapi::PrimitiveTopology::TriangleListWithAdjacency:
	case st::rapi::PrimitiveTopology::TriangleStripWithAdjacency: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	case st::rapi::PrimitiveTopology::PatchList: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	default:
		assert(0);
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

D3D12_PRIMITIVE_TOPOLOGY st::rapi::dx12::GetPrimitiveTopology(st::rapi::PrimitiveTopology primTopo, uint32_t controlPoints)
{
	switch (primTopo)
	{
	case st::rapi::PrimitiveTopology::PointList:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case st::rapi::PrimitiveTopology::LineList:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case st::rapi::PrimitiveTopology::LineStrip:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case st::rapi::PrimitiveTopology::TriangleList:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case st::rapi::PrimitiveTopology::TriangleStrip:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case st::rapi::PrimitiveTopology::TriangleFan:
		assert(0);
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	case st::rapi::PrimitiveTopology::TriangleListWithAdjacency:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case st::rapi::PrimitiveTopology::TriangleStripWithAdjacency:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	case st::rapi::PrimitiveTopology::PatchList:
		if (controlPoints == 0 || controlPoints > 32)
		{
			assert(0);
			return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}
		return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoints - 1));
	default:
		assert(0);
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}

D3D12_RENDER_PASS_BEGINNING_ACCESS st::rapi::dx12::GetRenderPassBeginningAccess(st::rapi::RenderPassOp::LoadOp op, const st::rapi::ClearValue& clearValue, st::rapi::Format format)
{
	D3D12_RENDER_PASS_BEGINNING_ACCESS ret = {};
	switch (op)
	{
	case st::rapi::RenderPassOp::LoadOp::Load: 
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		break;
	case st::rapi::RenderPassOp::LoadOp::Clear:
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		ret.Clear.ClearValue.Format = st::rapi::dx12::GetDxgiFormatMapping(format).resourceFormat;
		if (IsDepthFormat(format))
		{
			ret.Clear.ClearValue.DepthStencil.Depth = clearValue.depthStencil.depth;
			ret.Clear.ClearValue.DepthStencil.Stencil = clearValue.depthStencil.stencil;
		}
		else
		{
			ret.Clear.ClearValue.Color[0] = clearValue.color.x;
			ret.Clear.ClearValue.Color[1] = clearValue.color.y;
			ret.Clear.ClearValue.Color[2] = clearValue.color.z;
			ret.Clear.ClearValue.Color[3] = clearValue.color.w;
		}
		break;
	case st::rapi::RenderPassOp::LoadOp::Discard:
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
		break;
	default:
		assert(0);
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	}
	return ret;
}

D3D12_RENDER_PASS_ENDING_ACCESS st::rapi::dx12::GetRenderPassEngindAccess(st::rapi::RenderPassOp::StoreOp op)
{
	D3D12_RENDER_PASS_ENDING_ACCESS ret = {};
	switch (op)
	{
	case st::rapi::RenderPassOp::StoreOp::Store:
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		break;
	case st::rapi::RenderPassOp::StoreOp::Discard:
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
		break;
	default:
		assert(0);
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	}
	return ret;
}