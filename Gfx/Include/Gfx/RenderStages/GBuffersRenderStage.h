#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{
class GBuffersRenderStage : public RenderStage
{
public:

	GBuffersRenderStage() = default;

	const char* GetDebugName() const override { return "GBuffersRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	st::rhi::ShaderOwner m_VS;
	st::rhi::ShaderOwner m_PS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::gfx::RenderContext m_RenderContext;
};

} // namespace st::gfx