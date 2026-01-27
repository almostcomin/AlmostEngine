#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

class ToneMappingRenderStage : public RenderStage
{
public:

	static constexpr int c_NumHistogramBins = 256;

public:

	ToneMappingRenderStage() = default;

	void SetExposure(float v) { m_Exposure = v; }
	void SetTonemapping(bool v) { m_Tonemapping = v; }

	const char* GetDebugName() const override { return "ToneMappingRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

private:

	rhi::ShaderOwner m_BuildHistogramCS;
	rhi::ComputePipelineStateOwner m_BuildHistogramPSO;

	rhi::ShaderOwner m_CS;
	rhi::ComputePipelineStateOwner m_PSO;

	float m_Exposure = 1.f;
	bool m_Tonemapping = true;
};

} // namespace st::gfx