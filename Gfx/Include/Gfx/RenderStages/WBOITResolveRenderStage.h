#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class WBOITResolveRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(WBOITResolveRenderStage)

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_AccumWOITTexture;
	RGTextureHandle m_RevealageWOITTexture;
	RGTextureHandle m_SceneColorTexture;
	
	alm::rhi::ShaderOwner m_PS;
	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::rhi::GraphicsPipelineStateOwner m_PSO;
};

} // namespace st::gfx