#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/dx12/Utils.h"
#include <cassert>

namespace
{
	D3D12_BLEND GetBlend(alm::rhi::BlendFactor factor)
	{
		switch (factor)
		{
		case alm::rhi::BlendFactor::Zero: return D3D12_BLEND_ZERO;
		case alm::rhi::BlendFactor::One: return D3D12_BLEND_ONE;
		case alm::rhi::BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
		case alm::rhi::BlendFactor::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
		case alm::rhi::BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
		case alm::rhi::BlendFactor::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
		case alm::rhi::BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
		case alm::rhi::BlendFactor::InvDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
		case alm::rhi::BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
		case alm::rhi::BlendFactor::InvDstColor: return D3D12_BLEND_INV_DEST_COLOR;
		case alm::rhi::BlendFactor::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
		case alm::rhi::BlendFactor::ConstantColor: return D3D12_BLEND_BLEND_FACTOR;
		case alm::rhi::BlendFactor::InvConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
		case alm::rhi::BlendFactor::Src1Color: return D3D12_BLEND_SRC1_COLOR;
		case alm::rhi::BlendFactor::InvSrc1Color: return D3D12_BLEND_INV_SRC1_COLOR;
		case alm::rhi::BlendFactor::Src1Alpha: return D3D12_BLEND_SRC1_ALPHA;
		case alm::rhi::BlendFactor::InvSrc1Alpha: return D3D12_BLEND_INV_SRC1_ALPHA;
		default:
			assert(0);
			return D3D12_BLEND_ZERO;
		};
	}

	D3D12_BLEND_OP GetBlendOp(alm::rhi::BlendOp op)
	{
		switch (op)
		{
		case alm::rhi::BlendOp::Add: return D3D12_BLEND_OP_ADD;
		case alm::rhi::BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
		case alm::rhi::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
		case alm::rhi::BlendOp::Min: return D3D12_BLEND_OP_MIN;
		case alm::rhi::BlendOp::Max: return D3D12_BLEND_OP_MAX;
		default:
			assert(0);
			return D3D12_BLEND_OP_ADD;
		}
	}

	D3D12_FILL_MODE GetFillMode(alm::rhi::FillMode mode)
	{
		switch (mode)
		{
		case alm::rhi::FillMode::Solid: return D3D12_FILL_MODE_SOLID;
		case alm::rhi::FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		default:
			assert(0);
			return D3D12_FILL_MODE_SOLID;
		}
	}

	D3D12_CULL_MODE GetCullMode(alm::rhi::CullMode mode)
	{
		switch (mode)
		{
		case alm::rhi::CullMode::Back: return D3D12_CULL_MODE_BACK;
		case alm::rhi::CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case alm::rhi::CullMode::None: return D3D12_CULL_MODE_NONE;
		default:
			assert(0);
			return D3D12_CULL_MODE_BACK;
		}
	}

	D3D12_COMPARISON_FUNC GetComparisonFunc(alm::rhi::ComparisonFunc func)
	{
		switch (func)
		{
		case alm::rhi::ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case alm::rhi::ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
		case alm::rhi::ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case alm::rhi::ComparisonFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case alm::rhi::ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case alm::rhi::ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case alm::rhi::ComparisonFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case alm::rhi::ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		default:
			assert(0);
			return D3D12_COMPARISON_FUNC_NEVER;
		}
	}

	D3D12_STENCIL_OP GetStencilOp(alm::rhi::StencilOp op)
	{
		switch (op)
		{
		case alm::rhi::StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
		case alm::rhi::StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
		case alm::rhi::StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
		case alm::rhi::StencilOp::IncrementAndClamp: return D3D12_STENCIL_OP_INCR_SAT;
		case alm::rhi::StencilOp::DecrementAndClamp: return D3D12_STENCIL_OP_DECR_SAT;
		case alm::rhi::StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
		case alm::rhi::StencilOp::IncrementAndWrap: return D3D12_STENCIL_OP_INCR;
		case alm::rhi::StencilOp::DecrementAndWrap: return D3D12_STENCIL_OP_DECR;
		default:
			assert(0);
			return D3D12_STENCIL_OP_KEEP;
		}
	}

} // anonymous namespace

D3D12_BLEND_DESC alm::rhi::dx12::GetBlendDesc(const alm::rhi::BlendState& desc)
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

D3D12_RASTERIZER_DESC alm::rhi::dx12::GetRasterizerState(const alm::rhi::RasterizerState& rasterState)
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

D3D12_DEPTH_STENCIL_DESC alm::rhi::dx12::GetDepthStencilState(const alm::rhi::DepthStencilState& depthStencilState)
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

D3D12_PRIMITIVE_TOPOLOGY_TYPE alm::rhi::dx12::GetPrimitiveType(alm::rhi::PrimitiveTopology primTopo)
{
	switch (primTopo)
	{
	case alm::rhi::PrimitiveTopology::PointList: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	case alm::rhi::PrimitiveTopology::LineList:
	case alm::rhi::PrimitiveTopology::LineStrip: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	case alm::rhi::PrimitiveTopology::TriangleList:
	case alm::rhi::PrimitiveTopology::TriangleStrip:
	case alm::rhi::PrimitiveTopology::TriangleFan:
	case alm::rhi::PrimitiveTopology::TriangleListWithAdjacency:
	case alm::rhi::PrimitiveTopology::TriangleStripWithAdjacency: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	case alm::rhi::PrimitiveTopology::PatchList: 
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	default:
		assert(0);
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

D3D12_PRIMITIVE_TOPOLOGY alm::rhi::dx12::GetPrimitiveTopology(alm::rhi::PrimitiveTopology primTopo, uint32_t controlPoints)
{
	switch (primTopo)
	{
	case alm::rhi::PrimitiveTopology::PointList:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case alm::rhi::PrimitiveTopology::LineList:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case alm::rhi::PrimitiveTopology::LineStrip:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case alm::rhi::PrimitiveTopology::TriangleList:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case alm::rhi::PrimitiveTopology::TriangleStrip:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case alm::rhi::PrimitiveTopology::TriangleFan:
		assert(0);
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	case alm::rhi::PrimitiveTopology::TriangleListWithAdjacency:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case alm::rhi::PrimitiveTopology::TriangleStripWithAdjacency:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	case alm::rhi::PrimitiveTopology::PatchList:
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

D3D12_RENDER_PASS_BEGINNING_ACCESS alm::rhi::dx12::GetRenderPassBeginningAccess(alm::rhi::RenderPassOp::LoadOp op, const alm::rhi::ClearValue& clearValue, alm::rhi::Format format)
{
	D3D12_RENDER_PASS_BEGINNING_ACCESS ret = {};
	switch (op)
	{
	case alm::rhi::RenderPassOp::LoadOp::NoAccess:
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
		break;
	case alm::rhi::RenderPassOp::LoadOp::Load: 
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		break;
	case alm::rhi::RenderPassOp::LoadOp::Clear:
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		ret.Clear.ClearValue.Format = alm::rhi::GetDxgiFormatMapping(format).typelessFormat;
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
	case alm::rhi::RenderPassOp::LoadOp::Discard:
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
		break;
	default:
		assert(0);
		ret.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	}
	return ret;
}

D3D12_RENDER_PASS_ENDING_ACCESS alm::rhi::dx12::GetRenderPassEngindAccess(alm::rhi::RenderPassOp::StoreOp op)
{
	D3D12_RENDER_PASS_ENDING_ACCESS ret = {};
	switch (op)
	{
	case alm::rhi::RenderPassOp::StoreOp::NoAccess:
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
		break;
	case alm::rhi::RenderPassOp::StoreOp::Store:
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		break;
	case alm::rhi::RenderPassOp::StoreOp::Discard:
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
		break;
	default:
		assert(0);
		ret.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	}
	return ret;
}

void alm::rhi::dx12::WaitForFence(ID3D12Fence* fence, uint64_t value, HANDLE event)
{
	// Test if the fence has been reached
	if (fence->GetCompletedValue() < value)
	{
		// If it's not, wait for it to finish using an event
		ResetEvent(event);
		fence->SetEventOnCompletion(value, event);
		WaitForSingleObject(event, INFINITE);
	}
}