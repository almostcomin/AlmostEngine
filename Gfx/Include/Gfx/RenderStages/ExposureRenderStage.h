#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class ExposureRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(ExposureRenderStage)

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

	ExposureRenderStage();

	float GetMinLogLuminance() const { return m_MinLogLuminance; }
	float GetLogLuminanceRange() const { return m_LogLuminanceRange; }

	void SetMinLogLuminance(float v) { m_MinLogLuminance = v; }
	void SetLogLuminanceRange(float v) { m_LogLuminanceRange = v; }

	void SetAdaptationUpSpeed(float v) { m_AdaptationUpSpeed = v; }
	void SetAdaptationDownSpeed(float v) { m_AdaptationDownSpeed = v; }

	float GetAdaptationUpSpeed() { return m_AdaptationUpSpeed; }
	float GetAdaptationDownSpeed() { return m_AdaptationDownSpeed; }

	void SetMiddleGray(float v) { m_MiddleGray = v; }
	float GetMiddleGray() const { return m_MiddleGray; }

	void SetSDRExposureBias(float v) { m_SdrExposureBias = v; }
	float GetSDRExposureBias() const { return m_SdrExposureBias; }

	Stats GetStats();

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGBufferHandle m_LuminanceHistogramBuffer;
	RGTextureHandle m_LuminanceAverageTexture;
	RGTextureHandle m_SceneColorTexture;
	RGTextureHandle m_ExposureRatioTexture;

	rhi::TextureOwner m_ExposureTexture[2];
	int m_ExposureTextureIndex = -1;

	rhi::ShaderOwner m_BuildHistogramCS;
	rhi::ComputePipelineStateOwner m_BuildHistogramPSO;

	rhi::ShaderOwner m_ComputeExposureCS;
	rhi::ComputePipelineStateOwner m_ComputeExposurePSO;

	float m_MinLogLuminance;
	float m_LogLuminanceRange;

	float m_AdaptationUpSpeed = 2.f;
	float m_AdaptationDownSpeed = 0.75f;

	float m_MiddleGray = 0.18f;
	float m_SdrExposureBias = 0.8f;

	rhi::BufferOwner m_StatsBuffer;
	rhi::BufferOwner m_StatsBufferReadBack;
};

} // namespace alm::gfx