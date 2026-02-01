#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

class ToneMappingRenderStage : public RenderStage
{
public:

	static constexpr int c_NumHistogramBins = 256;

public:

	ToneMappingRenderStage();

	void SetExposure(float v) { m_Exposure = v; }
	void SetTonemapping(bool v) { m_Tonemapping = v; }

	float GetMinLogLuminance() const { return m_MinLogLuminance; }
	float GetLogLuminanceRange() const { return m_LogLuminanceRange; }

	void SetMinLogLuminance(float v) { m_MinLogLuminance = v; }
	void SetLogLuminanceRange(float v) { m_LogLuminanceRange = v; }

	const char* GetDebugName() const override { return "ToneMappingRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

private:

	rhi::ShaderOwner m_BuildHistogramCS;
	rhi::ComputePipelineStateOwner m_BuildHistogramPSO;

	rhi::ShaderOwner m_AvgLuminanceCS;
	rhi::ComputePipelineStateOwner m_AvgLuminancePSO;

	rhi::ShaderOwner m_CS;
	rhi::ComputePipelineStateOwner m_PSO;

	float m_Exposure = 1.f;
	bool m_Tonemapping = true;

	float m_MinLogLuminance;
	float m_LogLuminanceRange;
};

} // namespace st::gfx