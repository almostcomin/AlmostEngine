#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class LoadedTexture;

class CubemapSkyRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(CubemapSkyRenderStage)

public:

    void SetCubemapTexture(std::shared_ptr<alm::gfx::LoadedTexture> cubemap);
    std::shared_ptr<alm::gfx::LoadedTexture> GetCubemapTexture() const;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_SceneDepthTexture;
    RGFramebufferHandle m_FB;

    rhi::ShaderOwner m_PS;
    rhi::GraphicsPipelineStateOwner m_PSO;

    std::shared_ptr<alm::gfx::LoadedTexture> m_CubemapTexture;
};

} // namespace alm::gfx