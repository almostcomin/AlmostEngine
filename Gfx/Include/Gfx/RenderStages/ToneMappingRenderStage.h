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

	void TonemapHDR(alm::rhi::CommandListHandle commandList);
	void TonemapSDR(alm::rhi::CommandListHandle commandList);

private:

	RGTextureHandle m_ToneMappedTexture;
	RGTextureHandle m_ExposedColorTexture;
	RGTextureHandle m_BloomResultTexture;

	rhi::ShaderOwner m_TonemappingSDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingSDR_PSO;

	rhi::ShaderOwner m_TonemappingHDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingHDR_PSO;

	bool m_TonemappingEnabled = true;
};

} // namespace st::gfx