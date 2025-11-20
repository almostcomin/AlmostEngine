#pragma once

#include "RenderAPI/BlendState.h"
#include "RenderAPI/RasterizerState.h"
#include "RenderAPI/DepthStencilState.h"
#include "RenderAPI/PipelineState.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/dx12/d3d12_headers.h"

namespace st::rapi::dx12
{
	D3D12_BLEND_DESC GetBlendDesc(const st::rapi::BlendState& desc);
	D3D12_RASTERIZER_DESC GetRasterizerState(const st::rapi::RasterizerState& rasterizerState);
	D3D12_DEPTH_STENCIL_DESC GetDepthStencilState(const st::rapi::DepthStencilState& rasterizerState);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimitiveType(st::rapi::PrimitiveTopology primTopo);
	D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(st::rapi::PrimitiveTopology primTopo, uint32_t controlPoints);
	D3D12_RENDER_PASS_BEGINNING_ACCESS GetRenderPassBeginningAccess(st::rapi::RenderPassOp::LoadOp op, const ClearValue& clearValue, Format format);
	D3D12_RENDER_PASS_ENDING_ACCESS GetRenderPassEngindAccess(st::rapi::RenderPassOp::StoreOp op);

} // namespace st::rpai::dx12