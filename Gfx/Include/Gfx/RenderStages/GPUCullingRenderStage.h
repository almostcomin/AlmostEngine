#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

class GPUCullingRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(GPUCullingRenderStage)

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGBufferHandle m_PayloadBuffer;
	RGBufferHandle m_IndirectArgsBuffer;

	alm::rhi::ShaderOwner m_PrepareCS;
	alm::rhi::ShaderOwner m_CullingCS;

	alm::rhi::ComputePipelineStateOwner m_PreparePSO;
	alm::rhi::ComputePipelineStateOwner m_CullingPSO;
};

} // namespace alm::gfx