#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

class ToneMappingRenderStage : public RenderStage
{
public:

	static constexpr int c_NumHistogramBins = 256;

	struct Stats
	{
		float minLuminance;
		float maxLuminance;
		float avgLuminance;
		float avgBin;
		float totalPixels;
	};

public:

	ToneMappingRenderStage();

	void SetTonemappingEnabled(bool v) { m_TonemappingEnabled = v; }

	void SetSceneMiddleGray(float v) { m_SceneMiddleGray = v; }
	float GetSceneMiddleGray() const { return m_SceneMiddleGray; }
	
	void SetSDRExposureBias(float v) { m_sdrExposureBias = v; }
	float GetSDRExposureBias() const { return m_sdrExposureBias; }

	float GetMinLogLuminance() const { return m_MinLogLuminance; }
	float GetLogLuminanceRange() const { return m_LogLuminanceRange; }

	void SetMinLogLuminance(float v) { m_MinLogLuminance = v; }
	void SetLogLuminanceRange(float v) { m_LogLuminanceRange = v; }

	const char* GetDebugName() const override { return "ToneMappingRenderStage"; }

	Stats GetStats();

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

	void TonemapHDR();
	void TonemapSDR();

private:

	rhi::ShaderOwner m_BuildHistogramCS;
	rhi::ComputePipelineStateOwner m_BuildHistogramPSO;

	rhi::ShaderOwner m_AvgLuminanceCS;
	rhi::ComputePipelineStateOwner m_AvgLuminancePSO;

	rhi::ShaderOwner m_TonemappingSDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingSDR_PSO;

	rhi::ShaderOwner m_TonemappingHDR_CS;
	rhi::ComputePipelineStateOwner m_TonemappingHDR_PSO;

	bool m_TonemappingEnabled = true;
	float m_SceneMiddleGray = 0.18f;
	float m_sdrExposureBias = 0.8f;

	float m_MinLogLuminance;
	float m_LogLuminanceRange;

	rhi::BufferOwner m_StatsBuffer;
	rhi::BufferOwner m_StatsBufferReadBack;
};

} // namespace st::gfx