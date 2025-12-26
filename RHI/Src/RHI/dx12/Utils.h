#pragma once

#include "RHI/BlendState.h"
#include "RHI/RasterizerState.h"
#include "RHI/DepthStencilState.h"
#include "RHI/PipelineState.h"
#include "RHI/CommandList.h"
#include "RHI/dx12/d3d12_headers.h"

namespace st::rhi::dx12
{
	D3D12_BLEND_DESC GetBlendDesc(const st::rhi::BlendState& desc);
	D3D12_RASTERIZER_DESC GetRasterizerState(const st::rhi::RasterizerState& rasterizerState);
	D3D12_DEPTH_STENCIL_DESC GetDepthStencilState(const st::rhi::DepthStencilState& rasterizerState);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimitiveType(st::rhi::PrimitiveTopology primTopo);
	D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(st::rhi::PrimitiveTopology primTopo, uint32_t controlPoints);
	D3D12_RENDER_PASS_BEGINNING_ACCESS GetRenderPassBeginningAccess(st::rhi::RenderPassOp::LoadOp op, const ClearValue& clearValue, Format format);
	D3D12_RENDER_PASS_ENDING_ACCESS GetRenderPassEngindAccess(st::rhi::RenderPassOp::StoreOp op);

} // namespace st::rpai::dx12