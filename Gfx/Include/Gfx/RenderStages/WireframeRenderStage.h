#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{

class WireframeRenderStage : public RenderStage
{
public:

	WireframeRenderStage() = default;

	const char* GetDebugName() const override { return "WireframeRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	st::rhi::ShaderOwner m_VS;
	st::rhi::ShaderOwner m_PS;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::gfx::RenderContext m_RenderContext;

	st::rhi::FramebufferOwner m_FB;
};

} // namespace st::gfx