#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class LinearizeDepthRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(LinearizeDepthRenderStage)

public:

	LinearizeDepthRenderStage() = default;

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGTextureHandle m_LinearDepthTexture;
	RGTextureHandle m_SceneDepthTexture;

	rhi::ShaderOwner m_LinearizeDepthCS;
	rhi::ComputePipelineStateOwner m_LinearizeDepthPSO;
};

} // namespace st::gfx