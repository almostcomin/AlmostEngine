#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{

class DepthPrepassRenderStage : public RenderStage
{
public:

	DepthPrepassRenderStage() = default;

	const char* GetDebugName() const override { return "DepthPrepassRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	st::rhi::ShaderOwner m_VS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::rhi::GraphicsPipelineStateOwner m_PSO;
};

} // namespace st::gfx