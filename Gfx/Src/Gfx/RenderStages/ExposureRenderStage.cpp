#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/ExposureRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

alm::gfx::ExposureRenderStage::ExposureRenderStage()
{
	// [-10..+2] log range
	m_MinLogLuminance = -10.f;			// exp2(-10) = 0.0009765625 -> min luminance
	m_LogLuminanceRange = 12;			// exp2(-10 + 12) = exp2(2) = 4 -> max luminance
}

alm::gfx::ExposureRenderStage::Stats alm::gfx::ExposureRenderStage::GetStats()
{
	Stats result = {};

	const auto* ptr = (interop::TonemappingStatsBuffer*)m_StatsBufferReadBack->Map();

	result.minLuminance = ptr->minLuminance;
	result.maxLuminance = ptr->maxLuminance;
	result.avgLuminance = ptr->avgLuminance;
	result.avgBin = ptr->avgBin;

	m_StatsBufferReadBack->Unmap();

	alm::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ExposedColorTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	result.totalPixels = width * height;

	return result;
}

void alm::gfx::ExposureRenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create resources
	{
		m_LuminanceHistogramBuffer = builder.CreateBuffer("LuminanceHistogram", rhi::BufferDesc{
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly | rhi::BufferShaderUsage::ReadWrite,
			.sizeBytes = 256 * sizeof(uint32_t) });

		m_LuminanceAverageTexture = builder.CreateTexture("LuminanceAverage", RenderGraph::TextureResourceType::ShaderResource,
			1, 1, 1, rhi::Format::R32_FLOAT, true);

		m_ExposureRatioTexture = builder.CreateTexture("ExposureRatio", RenderGraph::TextureResourceType::ShaderResource,
			1, 1, 1, rhi::Format::R32_FLOAT, true);

		m_ExposedColorTexture = builder.CreateColorTarget("ExposedColor", 1, RenderGraph::SizeSpace::SceneViewport, 1,
			rhi::Format::RGBA16_FLOAT, true);

		m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	}

	// Request resources access
	{
		builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddBufferDependency(m_LuminanceHistogramBuffer, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_LuminanceAverageTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_ExposureRatioTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_ExposedColorTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}
}

void alm::gfx::ExposureRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	DeviceManager* deviceManager = GetDeviceManager();
	CommonResources* commonResources = deviceManager->GetCommonResources();
	UploadBuffer* uploadBuffer = deviceManager->GetUploadBuffer();

	alm::rhi::TextureHandle inputTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);
	alm::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ExposedColorTexture);
	alm::rhi::BufferHandle histogramBuffer = m_RenderGraph->GetBuffer(m_LuminanceHistogramBuffer);
	alm::rhi::TextureHandle avgLuminanceTexture = m_RenderGraph->GetTexture(m_LuminanceAverageTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;
	assert(width == inputTexture->GetDesc().width);
	assert(height == inputTexture->GetDesc().height);

	if (m_ExposureTextureIndex < 0)
	{
		commonResources->ClearTexture2D_R(commandList.get(), m_ExposureTexture[0].get(), 1.f,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		commonResources->ClearTexture2D_R(commandList.get(), m_ExposureTexture[1].get(), 1.f,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		commonResources->ClearTexture2D_R(commandList.get(), avgLuminanceTexture.get(), 1.f,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		m_ExposureTextureIndex = 0;
	}
	int prevExposureTextureIndex = m_ExposureTextureIndex;
	m_ExposureTextureIndex = (m_ExposureTextureIndex + 1) % 2;

	// Init stats buffer
	// TODO: Do this only if required
	{
		commandList->BeginMarker("Init stats");

		auto [initialStats, offset] = uploadBuffer->RequestSpaceForBufferDataUpload<interop::TonemappingStatsBuffer>();

		initialStats->minLuminance = FLT_MAX;
		initialStats->maxLuminance = 0.f;
		initialStats->avgLuminance = 0.f;
		initialStats->avgBin = 0.f;

		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST));
		commandList->CopyBufferToBuffer(m_StatsBuffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, sizeof(interop::TonemappingStatsBuffer));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::UNORDERED_ACCESS));

		commandList->EndMarker();
	}

	// Clear luminance histogram buffer
	{
		commandList->BeginMarker("Clear histogram");
		commandList->SetPipelineState(commonResources->GetClearBufferPSO().get());

		interop::ClearBufferConstants shaderConstants;
		shaderConstants.bufferDI = histogramBuffer->GetReadWriteView();
		const uint32_t elemCount = histogramBuffer->GetDesc().sizeBytes / 4;
		shaderConstants.bufferElementCount = elemCount;
		shaderConstants.clearValue = 0;

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch((elemCount + 255) / 256, 1, 1);

		commandList->EndMarker();
	}

	/*
	* Some considerations:
	*
	* logLuminanceRage: rage of luminance in logarithmic space to be captured:
	*	minLuminance = 0.001	-> log2(0.001) = -9.97
	*	maxLuminance = 64.0		-> log2(64.0)  = 6.0
	*	logLuminanceRange = 6.0	-> -10.0 = 16.0
	*/

	// First pass, build luminance histogram
	{
		commandList->BeginMarker("Build histogram");
		commandList->PushBarrier(rhi::Barrier::Memory(histogramBuffer.get()));
		commandList->SetPipelineState(m_BuildHistogramPSO.get());

		interop::BuildLuminanceHistogramConstants shaderConstants;
		shaderConstants.inputTextureDI = inputTexture->GetSampledView();
		shaderConstants.outputHistogramBufferDI = histogramBuffer->GetReadWriteView();
		shaderConstants.outputStatsBufferDI = m_StatsBuffer->GetReadWriteView();
		shaderConstants.viewBegin = uint2{ 0 };
		shaderConstants.viewEnd = uint2{ width, height };
		shaderConstants.minLogLuminance = m_MinLogLuminance;
		shaderConstants.oneOverLogLuminanceRange = 1.f / m_LogLuminanceRange;

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

		commandList->EndMarker();
	}

	// Compute exposure
	{
		commandList->BeginMarker("Compute exposure");

		commandList->PushBarriers({
			rhi::Barrier::Memory(histogramBuffer.get()),
			rhi::Barrier::Memory(m_StatsBuffer.get()),
			rhi::Barrier::Texture(m_ExposureTexture[m_ExposureTextureIndex].get(),
				rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS) });

		commandList->SetPipelineState(m_ComputeExposurePSO.get());

		interop::ComputeExposureConstants shaderConstants;
		shaderConstants.inputHistogramBufferDI = histogramBuffer->GetReadOnlyView();
		shaderConstants.inputPrevExposureTextureDI = m_ExposureTexture[prevExposureTextureIndex]->GetSampledView();
		shaderConstants.outputAvgLuminanceTextureDI = avgLuminanceTexture->GetStorageView();
		shaderConstants.outputExposureTextureDI = m_ExposureTexture[m_ExposureTextureIndex]->GetStorageView();
		shaderConstants.outputExposureRatioTextureDI = m_RenderGraph->GetTextureStorageView(m_ExposureRatioTexture);
		shaderConstants.outputStatsBufferDI = m_StatsBuffer->GetReadWriteView();
		shaderConstants.pixelCount = width * height;
		shaderConstants.minLogLuminance = m_MinLogLuminance;
		shaderConstants.logLuminanceRange = m_LogLuminanceRange;
		shaderConstants.timeDelta = GetRenderView()->GetTimeDelta();
		shaderConstants.adaptionSpeedUp = m_AdaptationUpSpeed;
		shaderConstants.adaptionSpeedDown = m_AdaptationDownSpeed;
		shaderConstants.middleGray = m_MiddleGray;
		shaderConstants.sdrExposureBias = m_SdrExposureBias;

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(1, 1, 1);

		commandList->EndMarker();
	}

	// Apply exposure
	{
		commandList->BeginMarker("Apply exposure");

		commandList->PushBarrier(rhi::Barrier::Texture(m_ExposureTexture[m_ExposureTextureIndex].get(),
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

		commandList->SetPipelineState(m_ApplyExposurePSO.get());

		interop::ApplyExposureConstants shaderConstants;
		shaderConstants.InputSceneColorTextureDI = inputTexture->GetSampledView();
		shaderConstants.InputExposureTextureDI = m_ExposureTexture[m_ExposureTextureIndex]->GetSampledView();
		shaderConstants.OutputExposedColorTextureDI = outputTexture->GetStorageView();
		shaderConstants.TextureDim = uint2{ width, height };

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

		commandList->EndMarker();
	}

	// Retrieve stats
	// TODO: Do this only if required
	{
		commandList->PushBarriers({
			rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::COPY_SRC),
			rhi::Barrier::Buffer(m_StatsBufferReadBack.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST) });

		commandList->CopyBufferToBuffer(m_StatsBufferReadBack.get(), 0, m_StatsBuffer.get(), 0, sizeof(interop::TonemappingStatsBuffer));

		commandList->PushBarriers({
			rhi::Barrier::Buffer(m_StatsBufferReadBack.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::COMMON),
			rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COPY_SRC, rhi::ResourceState::COMMON) });
	}
}

void alm::gfx::ExposureRenderStage::OnAttached()
{
	DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();

		m_BuildHistogramCS = shaderFactory->LoadShader("BuildLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_ComputeExposureCS = shaderFactory->LoadShader("ComputeExposure_cs", rhi::ShaderType::Compute);
		m_ApplyExposureCS = shaderFactory->LoadShader("ApplyExposure_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_BuildHistogramPSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BuildHistogramCS.get_weak() }, "BuildHistogramPSO");
		m_ComputeExposurePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ComputeExposureCS.get_weak() }, "ComputeExposurePSO");
		m_ApplyExposurePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ApplyExposureCS.get_weak() }, "ApplyExposurePSO");
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			rhi::TextureDesc desc = {
				.width = 1,
				.height = 1,
				.format = rhi::Format::R32_FLOAT,
				.shaderUsage = rhi::TextureShaderUsage::Sampled | rhi::TextureShaderUsage::Storage };

			m_ExposureTexture[i] = device->CreateTexture(desc, rhi::ResourceState::SHADER_RESOURCE, std::format("ExposureTexture[{}]", i));
		}
		m_ExposureTextureIndex = -1;
	}

	// Stats
	{
		{
			auto desc = rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
				.sizeBytes = sizeof(interop::TonemappingStatsBuffer),
				.format = rhi::Format::UNKNOWN,
				.stride = sizeof(interop::TonemappingStatsBuffer) };

			m_StatsBuffer = device->CreateBuffer(desc, rhi::ResourceState::COMMON, "TonemappingStats");
		}
		{
			auto desc = rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Readback,
				.shaderUsage = rhi::BufferShaderUsage::None,
				.sizeBytes = sizeof(interop::TonemappingStatsBuffer),
				.format = rhi::Format::UNKNOWN,
				.stride = 0 };

			m_StatsBufferReadBack = device->CreateBuffer(desc, rhi::ResourceState::COMMON, "TonemappingStatsRB");
		}
	}
}

void alm::gfx::ExposureRenderStage::OnDetached()
{
	m_StatsBufferReadBack = nullptr;
	m_StatsBuffer = nullptr;

	m_ApplyExposurePSO = nullptr;
	m_ComputeExposurePSO = nullptr;
	m_BuildHistogramPSO = nullptr;

	m_ApplyExposureCS = nullptr;
	m_ComputeExposureCS = nullptr;
	m_BuildHistogramCS = nullptr;
}