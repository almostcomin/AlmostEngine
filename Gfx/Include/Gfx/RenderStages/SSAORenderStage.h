#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

class SSAORenderStage : public RenderStage
{
public:

	SSAORenderStage() = default;

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

	virtual const char* GetDebugName() const { return "SSAORenderStage"; }

private:

	void CreateNoiseTexture();

private:

	rhi::ShaderOwner m_SSAO_CS;
	rhi::ComputePipelineStateOwner m_SSAO_PSO;

	rhi::TextureOwner m_NoiseTexture;
};

} // namespace st::gfx