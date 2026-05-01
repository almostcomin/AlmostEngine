#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

#define DEBUG_STAGE

namespace alm::gfx
{

class ShadowmapRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(ShadowmapRenderStage)

public:

	ShadowmapRenderStage();

	int2 GetSize() const { return int2{ m_TextureWidth, m_TextureHeight }; }
	void SetSize(const int2& textureSize);

	int GetDepthBias() const { return m_DepthBias; }
	void SetDepthBias(int v);

	float GetSlopeScaledDepthBias() const { return m_SlopeScaledDepthBias; }
	void SetSlopeScaledDepthBias(float v);

private:

	void InitResources();
	void ReleaseResources();

	void RecreatePSO();

	// Overrides
	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;
	void OnEnabled() override;
	void OnDisabled() override;

private:

	RGTextureHandle m_ShadowMapTexture;
#ifdef DEBUG_STAGE
	RGTextureHandle m_ShadowMapColorTexture;
#endif

	size_t m_TextureWidth;
	size_t m_TextureHeight;
	size_t m_NumCascades;
	rhi::Format m_PixelFormat;

	alm::rhi::ShaderOwner m_VS_Opaque;
	alm::rhi::ShaderOwner m_PS_Opaque;
	alm::rhi::ShaderOwner m_VS_AlphaTest;
	alm::rhi::ShaderOwner m_PS_AlphaTest;

	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::RenderContext m_RenderContext;

	int m_DepthBias;
	float m_SlopeScaledDepthBias;
};

} // namespace st::gfx