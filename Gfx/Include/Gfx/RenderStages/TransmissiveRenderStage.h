#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/MaterialPassRenderer.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class TransmissiveRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(TransmissiveRenderStage)

public:

	// Workload this pass submits, summed from the per-bucket culling stats over this pass's domains
	std::optional<MaterialPassRenderer::RenderStats> GetRenderedStats(const interop::CullingStats* stats) const override;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_SceneColorTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGTextureHandle m_ShadowmapTexture;
	RGTextureHandle m_AmbientOcclusionTexture;
	RGTextureHandle m_CloudsShadowmapTexture;
	RGBufferHandle m_PayloadBuffer;
	RGBufferHandle m_IndirectDrawArgsBuffer;
	RGBufferHandle m_IndirectDrawCountsBuffer;

	RGFramebufferHandle m_FB;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_PS;
	alm::rhi::ShaderOwner m_PS_AT;
	alm::gfx::MaterialPassRenderer m_MaterialPassRenderer;
	alm::gfx::MaterialPassRenderer::RenderStats m_LastRenderedStats;

	// Workload this stage submitted in its last draw, from the collected draw stats
	MaterialPassRenderer::RenderStats GetLastRenderedStats() const { return m_LastRenderedStats; }
};

} // namespace st::gfx