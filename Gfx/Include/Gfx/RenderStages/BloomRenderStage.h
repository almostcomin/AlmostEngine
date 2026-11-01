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
	void SetInternalBlend(float v) { m_InternalBlend = v; }
	void SetStrength(float v) { m_Strength = v; }
	void SetMipChainLength(uint32_t v);

	bool IsBloomEnabled() const { return m_BloomEnabled; }
	float GetFilterRadius() const { return m_FilterRadius; }
	float GetInternalBlend() const { return m_InternalBlend; }
	float GetStrength() const { return m_Strength; }
	uint32_t GetMipChainLength() const { return m_MipChainLength; }

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
	RGTextureHandle m_BloomResultTexture;
	RGFramebufferHandle m_FB;

	rhi::ComputePipelineStateOwner m_DownsamplePSO;
	rhi::ComputePipelineStateOwner m_UpsamplePSO;
	rhi::GraphicsPipelineStateOwner m_MixPSO;

	rhi::ShaderOwner m_DownsampleShader;
	rhi::ShaderOwner m_UpsampleShader;
	rhi::ShaderOwner m_MixShader;

	std::vector<rhi::TextureOwner> m_MipChain;

	bool m_BloomEnabled;
	float m_FilterRadius;
	float m_InternalBlend;
	float m_Strength;
	uint32_t m_MipChainLength;
};

} // namespace alm::gfx