#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{
class GBuffersRenderStage : public RenderStage
{
public:

	GBuffersRenderStage() = default;

	const char* GetDebugName() const override { return "GBuffersRenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_GBuffer0Texture;
	RGTextureHandle m_GBuffer1Texture;
	RGTextureHandle m_GBuffer2Texture;
	RGTextureHandle m_GBuffer3Texture;
	RGTextureHandle m_SceneDepthTexture;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_PS_Opaque;
	alm::rhi::ShaderOwner m_PS_AlphaTest;
	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::RenderContext m_RenderContext;
};

} // namespace st::gfx