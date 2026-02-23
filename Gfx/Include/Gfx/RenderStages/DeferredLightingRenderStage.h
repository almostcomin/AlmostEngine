#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"

namespace st::gfx
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

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	st::rhi::ShaderOwner m_PS;
	st::rhi::FramebufferOwner m_FB;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::rhi::GraphicsPipelineStateOwner m_PSO;

	MaterialChannel m_MaterialChannel = MaterialChannel::Disabled;
	bool m_ShowSSAO = false;
	bool m_ShowShadowmap = false;

};

} // namespace st::gfx