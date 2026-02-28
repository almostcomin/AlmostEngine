#include "Gfx/RenderContext.h"
#include "RHI/Device.h"
#include "RHI/PipelineState.h"

st::gfx::RenderContext st::gfx::CreateRenderContext(const st::rhi::GraphicsPipelineStateDesc& desc, const st::rhi::FramebufferInfo& fbInfo,
	st::rhi::Device* device, const std::string& baseDebugName)
{
	st::gfx::RenderContext renderContext;
	st::rhi::GraphicsPipelineStateDesc descCopy = desc;

	// Cull back
	descCopy.rasterState.cullMode = st::rhi::CullMode::Back;
	renderContext.PSO_BackCull = device->CreateGraphicsPipelineState(descCopy, fbInfo, std::format("{} - CullBack", baseDebugName));

	// Cull front	
	descCopy.rasterState.cullMode = st::rhi::CullMode::Front;
	renderContext.PSO_FrontCull = device->CreateGraphicsPipelineState(descCopy, fbInfo, std::format("{} - CullFront", baseDebugName));

	// Cull none
	descCopy.rasterState.cullMode = st::rhi::CullMode::None;
	renderContext.PSO_NoCull = device->CreateGraphicsPipelineState(descCopy, fbInfo, std::format("{} - CullNone", baseDebugName));

	return renderContext;
}