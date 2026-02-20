#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
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

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

private:

	void CreateNoiseTexture();

private:

	rhi::ShaderOwner m_SSAO_CS;
	rhi::ComputePipelineStateOwner m_SSAO_PSO;
	rhi::TextureOwner m_NoiseTexture;

	float m_Radius = 0.01f;
	float m_Power = 2.f;
	float m_Bias = 0.1f;

	bool m_SSAOEnabled = true;
};

} // namespace st::gfx