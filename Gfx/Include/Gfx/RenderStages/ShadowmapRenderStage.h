#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{

class ShadowmapRenderStage : public RenderStage
{
public:

	ShadowmapRenderStage(size_t resolution, size_t numCascades, rhi::Format pixelFormat);

	int2 GetSize() const { return int2{ m_TextureWidth, m_TextureHeight }; }
	void SetSize(const int2& textureSize);

private:

	void InitResources();
	void ReleaseResources();

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;
	void OnEnabled() override;
	void OnDisabled() override;

	virtual const char* GetDebugName() const { return "ShadowmapRenderStage"; }

private:

	size_t m_TextureWidth;
	size_t m_TextureHeight;
	size_t m_NumCascades;
	rhi::Format m_PixelFormat;

	st::rhi::ShaderOwner m_VS;
	st::rhi::ShaderOwner m_PS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::rhi::GraphicsPipelineStateOwner m_PSO;
};

} // namespace st::gfx