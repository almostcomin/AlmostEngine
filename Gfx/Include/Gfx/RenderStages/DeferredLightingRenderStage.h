#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

class DeferredLightingRenderStage : public RenderStage
{
public:

	enum class MaterialChannel
	{
		Disabled,
		BaseColor,
		Metalness,
		Anisotropy,
		Roughness,
		Scattering,
		Translucency,
		NormalMap,
		OcclusionMap,
		Emissive,
		SpecularF0
	};

public:

	DeferredLightingRenderStage() = default;

	void SetMaterialChannel(MaterialChannel v) { m_MaterialChannel = v; }
	void ShowSSAO(bool b) { m_ShowSSAO = b; }
	void ShowShadowmap(bool b) { m_ShowShadowmap = b; }

	const char* GetDebugName() const override { return "DeferredLightingRenderStage"; }

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
	RGTextureHandle m_GBuffer0Texture;
	RGTextureHandle m_GBuffer1Texture;
	RGTextureHandle m_GBuffer2Texture;
	RGTextureHandle m_GBuffer3Texture;
	RGTextureHandle m_AmbientOcclusionTexture;

	alm::rhi::ShaderOwner m_PS;
	alm::rhi::FramebufferOwner m_FB;
	alm::rhi::GraphicsPipelineStateOwner m_PSO;

	MaterialChannel m_MaterialChannel = MaterialChannel::Disabled;
	bool m_ShowSSAO = false;
	bool m_ShowShadowmap = false;

};

} // namespace st::gfx