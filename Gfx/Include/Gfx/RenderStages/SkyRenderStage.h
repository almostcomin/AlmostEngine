#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class SkyRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(SkyRenderStage)

public:

	struct SkyParams
	{
        float2 Offset = { 0.f, 0.f };
	};

public:

    const SkyParams& GetSkyParams() const { return m_Params; }
    void SetSkyParams(const SkyParams& params) { m_Params = params; }

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_SceneDepthTexture;
    RGFramebufferHandle m_FB;

    rhi::ShaderOwner m_PS;
    rhi::GraphicsPipelineStateOwner m_PSO;

    SkyParams m_Params;
};

} // namespace alm::gfx