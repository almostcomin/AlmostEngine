#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphBuilder.h"

namespace alm::gfx
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

	void SetAdaptationUpSpeed(float v) { m_AdaptationUpSpeed = v; }
	void SetAdaptationDownSpeed(float v) { m_AdaptationDownSpeed = v; }

	float GetAdaptationUpSpeed() { return m_AdaptationUpSpeed; }
	float GetAdaptationDownSpeed() { return m_AdaptationDownSpeed; }

	const char* GetDebugName() const override { return "ToneMappingRenderStage"; }

	Stats GetStats();

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

	void TonemapHDR(alm::rhi::CommandListHandle commandList);
	void TonemapSDR(alm::rhi::CommandListHandle commandList);

private:

	RGBufferHandle m_LuminanceHistogramBuffer;
	RGTextureHandle m_LuminanceAverageTexture;
	RGTextureHandle m_ToneMappedTexture;
	RGTextureHandle m_SceneColorTexture;

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

	float m_AdaptationUpSpeed = 2.f;
	float m_AdaptationDownSpeed = 0.75f;

	rhi::BufferOwner m_StatsBuffer;
	rhi::BufferOwner m_StatsBufferReadBack;
};

} // namespace st::gfx