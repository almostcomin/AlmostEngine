#pragma once

#include "Gfx/RenderContext.h"
#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"

namespace st::gfx
{

class DepthPrepassRenderStage : public RenderStage
{
public:

	DepthPrepassRenderStage() = default;

	const char* GetDebugName() const override { return "DepthPrepassRenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(st::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_SceneDepthTexture;

	st::rhi::ShaderOwner m_VS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::gfx::RenderContext m_RenderContext;
};

} // namespace st::gfx