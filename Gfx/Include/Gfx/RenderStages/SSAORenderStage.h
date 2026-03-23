#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

class SSAORenderStage : public RenderStage
{
public:

	SSAORenderStage() = default;

	bool IsSSAOEnabled() const { return m_SSAOEnabled; }
	void SetSSAOEnabled(bool b) { m_SSAOEnabled = b; }

	const float GetRadius() const { return m_Radius; }
	void SetRadius(float v) { m_Radius = v; }

	const float GetPower() const { return m_Power; }
	void SetPower(float v) { m_Power = v; }

	const float GetBias() const { return m_Bias; }
	void SetBias(float v) { m_Bias = v; }

	virtual const char* GetDebugName() const { return "SSAORenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

	void Passthrough(alm::rhi::CommandListHandle commandList);

private:

	RGTextureHandle m_AmbientOcclusionTexture;
	RGTextureHandle m_AOBlurTempTexture;
	RGTextureHandle m_LinearDepthTexture;
	RGTextureHandle m_GBuffer2Texture;

	rhi::ShaderOwner m_SSAO_CS;
	rhi::ComputePipelineStateOwner m_SSAO_PSO;

	rhi::ShaderOwner m_BilateralBlurV_CS;
	rhi::ComputePipelineStateOwner m_BilateralBlurV_PSO;

	rhi::ShaderOwner m_BilateralBlurH_CS;
	rhi::ComputePipelineStateOwner m_BilateralBlurH_PSO;

	float m_Radius = 0.05f;
	float m_Power = 4.f;
	float m_Bias = 0.1f;

	bool m_SSAOEnabled = true;
};

} // namespace st::gfx