#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "RHI/PipelineState.h"

namespace alm::gfx
{

class BloomRenderStage : public RenderStage
{
public:

	BloomRenderStage();

	virtual const char* GetDebugName() const { return "BloomRenderStage"; }

	void SetBloomEnabled(bool b) { m_BloomEnabled = b; }
	void SetFilterRadius(float v) { m_FilterRadius = v; }
	void SetStrength(float v) { m_Strength = v; }
	void SetMaxMipChainLenght(uint32_t v);

	float GetFilterRadius() const { return m_FilterRadius; }
	float GetStrength() const { return m_Strength; }
	uint32_t GetMaxMipChainLenght() const { return m_MipChainLength; }

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	void ReleaseMipChain(bool immediate);
	void ResetMipChain(bool immediate);

private:

	struct MipEntry
	{
		rhi::TextureOwner Texture;
		rhi::FramebufferOwner Framebuffer;
		rhi::GraphicsPipelineStateOwner PSO;
		rhi::GraphicsPipelineStateOwner BlendPSO;
	};

	RGTextureHandle m_SceneColorTexture;
	RGTextureHandle m_BloomResultTexture;
	RGFramebufferHandle m_FB;

	rhi::GraphicsPipelineStateDesc m_BlendPSODesc;
	rhi::GraphicsPipelineStateOwner m_PSO;

	rhi::ShaderOwner m_DownsampleShader;
	rhi::ShaderOwner m_UpsampleShader;
	rhi::ShaderOwner m_MixShader;

	std::vector<MipEntry> m_MipChain;

	bool m_BloomEnabled;
	float m_FilterRadius;
	float m_Strength;
	uint32_t m_MipChainLength;
};

} // namespace alm::gfx