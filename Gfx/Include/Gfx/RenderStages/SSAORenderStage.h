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

	bool m_SSAOEnabled = true;
};

} // namespace st::gfx