#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderContext.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"

namespace st::gfx
{

class ShadowmapRenderStage : public RenderStage
{
public:

	ShadowmapRenderStage(size_t resolution, size_t numCascades, rhi::Format pixelFormat);

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
	void Render(st::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;
	void OnEnabled() override;
	void OnDisabled() override;

	virtual const char* GetDebugName() const { return "ShadowmapRenderStage"; }

private:

	RGTextureHandle m_ShadowMapTexture;
	RGTextureHandle m_ShadowMapColorTexture;

	size_t m_TextureWidth;
	size_t m_TextureHeight;
	size_t m_NumCascades;
	rhi::Format m_PixelFormat;

	st::rhi::ShaderOwner m_VS_Opaque;
	st::rhi::ShaderOwner m_PS_Opaque;
	st::rhi::ShaderOwner m_VS_AlphaTest;
	st::rhi::ShaderOwner m_PS_AlphaTest;

	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::gfx::RenderContext m_RenderContext;

	int m_DepthBias;
	float m_SlopeScaledDepthBias;
};

} // namespace st::gfx