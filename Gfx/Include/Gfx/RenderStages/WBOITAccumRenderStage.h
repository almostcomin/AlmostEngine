#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class WBOITAccumRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(WBOITAccumRenderStage)

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_AccumWOITTexture;
	RGTextureHandle m_RevealageWOITTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGTextureHandle m_ShadowmapTexture;
	RGTextureHandle m_AmbientOcclusionTexture;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_PS;
	alm::rhi::FramebufferOwner m_FB;
	alm::gfx::RenderContext m_RenderContext;
};

} // namespace st::gfx