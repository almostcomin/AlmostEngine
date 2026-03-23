#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

class WireframeRenderStage : public RenderStage
{
public:

	WireframeRenderStage() = default;

	const char* GetDebugName() const override { return "WireframeRenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_ToneMappedTexture;
	RGTextureHandle m_SceneDepthTexture;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_PS;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::RenderContext m_RenderContext;

	alm::rhi::FramebufferOwner m_FB;
};

} // namespace st::gfx