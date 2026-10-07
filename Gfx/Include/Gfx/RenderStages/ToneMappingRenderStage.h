#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class ToneMappingRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(ToneMappingRenderStage)

public:

	ToneMappingRenderStage();

	void SetTonemappingEnabled(bool v) { m_TonemappingEnabled = v; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGTextureHandle m_ToneMappedTexture;
	RGTextureHandle m_BloomResultTexture;
	RGTextureHandle m_ExposureRatioTexture;

	rhi::ShaderOwner m_TonemappingSDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingSDR_PSO;

	rhi::ShaderOwner m_TonemappingHDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingHDR_PSO;

	bool m_TonemappingEnabled = true;
};

} // namespace st::gfx