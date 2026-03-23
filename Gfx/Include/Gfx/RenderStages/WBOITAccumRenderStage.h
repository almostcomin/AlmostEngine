#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

class WBOITAccumRenderStage : public RenderStage
{
public:

	const char* GetDebugName() const override { return "WBOITAccumRenderStage"; }

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