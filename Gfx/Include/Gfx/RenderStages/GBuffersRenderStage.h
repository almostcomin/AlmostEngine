#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/MaterialPassRenderer.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{
class GBuffersRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(GBuffersRenderStage)

public:

	enum class DebugChannel
	{
		Disabled,
		Heightmap_Heights,
		Heightmap_Slope,
		Heightmap_Normals,
		Heightmap_Connections,
		Heightmap_Resolution,
		Heightmap_Patches,
		Heightmap_Contours
	};

public:

	GBuffersRenderStage() = default;

	void SetDebugChannel(DebugChannel v)	{ m_DebugChannel = v; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_GBuffer0Texture;
	RGTextureHandle m_GBuffer1Texture;
	RGTextureHandle m_GBuffer2Texture;
	RGTextureHandle m_GBuffer3Texture;
	RGTextureHandle m_SceneDepthTexture;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_VS_Terrain;

	alm::rhi::ShaderOwner m_PS_Opaque;
	alm::rhi::ShaderOwner m_PS_AlphaTest;
	alm::rhi::ShaderOwner m_PS_Terrain;

	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateDesc m_PSODesc;
	alm::gfx::MaterialPassRenderer m_MaterialPassRenderer;

	DebugChannel m_DebugChannel = DebugChannel::Disabled;
};

} // namespace st::gfx