#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{

class ShadowmapRenderStage : public RenderStage
{
public:

	ShadowmapRenderStage(size_t resolution, size_t numCascades, rhi::Format pixelFormat);

	const char* GetDebugName() const override { return "ShadowmapRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

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