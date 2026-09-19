#pragma once

#include "Gfx/MaterialPassRenderer.h"
#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class DepthPrepassRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(DepthPrepassRenderStage)

public:

	DepthPrepassRenderStage() = default;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_SceneDepthTexture;
	RGBufferHandle m_PayloadBuffer;
	RGBufferHandle m_IndirectArgsBuffer;

	alm::rhi::ShaderOwner m_VS_Opaque;
	alm::rhi::ShaderOwner m_VS_AlphaTest;
	alm::rhi::ShaderOwner m_VS_Opaque_GpuCull;
	alm::rhi::ShaderOwner m_VS_AlphaTest_GpuCull;
	alm::rhi::ShaderOwner m_PS_AlphaTest;
	alm::rhi::ShaderOwner m_VS_Terrain;

	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::MaterialPassRenderer m_MaterialPassRenderer;
	alm::gfx::MaterialPassRenderer m_MaterialPassRenderer_GpuCull;
};

} // namespace st::gfx