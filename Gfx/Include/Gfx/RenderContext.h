#pragma once

namespace st::rhi
{
	struct GraphicsPipelineStateDesc;
	struct FramebufferInfo;
	class Device;
}

namespace st::gfx
{

struct RenderContext
{
	rhi::GraphicsPipelineStateOwner PSO_BackCull;
	rhi::GraphicsPipelineStateOwner PSO_FrontCull;
	rhi::GraphicsPipelineStateOwner PSO_NoCull;
};

RenderContext CreateRenderContext(const st::rhi::GraphicsPipelineStateDesc& desc, const st::rhi::FramebufferInfo& fbInfo, st::rhi::Device* device,
	const std::string& baseDebugName);

} //namespace st::gfx