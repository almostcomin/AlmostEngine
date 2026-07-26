#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class BloomRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(BloomRenderStage)

public:

	BloomRenderStage();

	void SetBloomEnabled(bool b) { m_BloomEnabled = b; }
	void SetFilterRadius(float v) { m_FilterRadius = v; }
	void SetStrength(float v) { m_Strength = v; }
	void SetThreshold(float v) { m_Threshold = v; }
	void SetThresholdKnee(float v) { m_Knee = v; }
	void SetMaxMipChainLenght(uint32_t v);

	float GetFilterRadius() const { return m_FilterRadius; }
	float GetStrength() const { return m_Strength; }
	float GetThreshold() const { return m_Threshold; }
	float GetThresholdKnee() const { return m_Knee; }
	uint32_t GetMaxMipChainLenght() const { return m_MipChainLength; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	void ReleaseMipChain(bool immediate);
	void ResetMipChain(bool immediate);

private:

	RGTextureHandle m_SceneColorTexture;
	RGTextureHandle m_BloomPrefilterTexture;
	RGTextureHandle m_BloomResultTexture;
	RGFramebufferHandle m_FB;

	rhi::ComputePipelineStateOwner m_DownsamplePSO;
	rhi::ComputePipelineStateOwner m_UpsamplePSO;
	rhi::GraphicsPipelineStateOwner m_MixPSO;
	rhi::ComputePipelineStateOwner m_FilterPSO;

	rhi::ShaderOwner m_DownsampleShader;
	rhi::ShaderOwner m_UpsampleShader;
	rhi::ShaderOwner m_MixShader;
	rhi::ShaderOwner m_FilterShader;

	std::vector<rhi::TextureOwner> m_MipChain;

	bool m_BloomEnabled;

	float m_FilterRadius;
	float m_Strength;
	float m_Threshold;
	float m_Knee;

	uint32_t m_MipChainLength;
};

} // namespace alm::gfx