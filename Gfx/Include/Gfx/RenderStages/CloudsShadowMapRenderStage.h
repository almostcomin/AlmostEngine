#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/MultiBuffer.h"

namespace alm::gfx
{

class CloudsShadowmapRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(CloudsShadowmapRenderStage)

public:

    CloudsShadowmapRenderStage();
    ~CloudsShadowmapRenderStage() = default;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGTextureHandle m_CloudsShadowmapTexture;
	RGTextureHandle m_LinearDepthTexture;

	rhi::ShaderOwner m_CS;
	rhi::ComputePipelineStateOwner m_PSO;

	gfx::MultiBuffer m_CloudsShadowmapCB;
};

} // namespace alm::gfx