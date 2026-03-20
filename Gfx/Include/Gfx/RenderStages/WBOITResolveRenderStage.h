#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "Gfx/RenderGraphTypes.h"

namespace st::gfx
{

class WBOITResolveRenderStage : public RenderStage
{
public:

	const char* GetDebugName() const override { return "WBOITResolveRenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(st::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_AccumWOITTexture;
	RGTextureHandle m_RevealageWOITTexture;
	RGTextureHandle m_SceneColorTexture;
	
	st::rhi::ShaderOwner m_PS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::rhi::GraphicsPipelineStateOwner m_PSO;
};

} // namespace st::gfx