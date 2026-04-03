#pragma once

#include "Gfx/RenderContext.h"
#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class DepthPrepassRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(DepthPrepassRenderStage)

public:

	DepthPrepassRenderStage() = default;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_SceneDepthTexture;

	alm::rhi::ShaderOwner m_VS_Opaque;
	alm::rhi::ShaderOwner m_VS_AlphaTest;
	alm::rhi::ShaderOwner m_PS_AlphaTest;
	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::RenderContext m_RenderContext;
};

} // namespace st::gfx