#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

st::gfx::ToneMappingRenderStage::ToneMappingRenderStage()
{
	// [-10..+2] log range
	m_MinLogLuminance = -10.f;			// exp2(-10) = 0.0009765625 -> min luminance
	m_LogLuminanceRange = 12;			// exp2(-10 + 12) = exp2(2) = 4 -> max luminance
}

st::gfx::ToneMappingRenderStage::Stats st::gfx::ToneMappingRenderStage::GetStats()
{
	Stats result = {};

	const auto* ptr = (interop::TonemappingStatsBuffer*)m_StatsBufferReadBack->Map();

	result.minLuminance = ptr->minLuminance;
	result.maxLuminance = ptr->maxLuminance;
	result.avgLuminance = ptr->avgLuminance;
	result.avgBin = ptr->avgBin;

	m_StatsBufferReadBack->Unmap();

	st::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ToneMappedTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	result.totalPixels = width * height;

	return result;
}

void st::gfx::ToneMappingRenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create resources
	{
		m_LuminanceHistogramBuffer = builder.CreateBuffer("LuminanceHistogram", rhi::BufferDesc{
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly | rhi::BufferShaderUsage::ReadWrite,
			.sizeBytes = 256 * sizeof(uint32_t) });

		m_LuminanceAverageTexture = builder.CreateTexture("LuminanceAverage", RenderGraph::TextureResourceType::ShaderResource, 
			1, 1, 1, rhi::Format::R32_FLOAT, true);

		m_ToneMappedTexture = builder.CreateTexture("ToneMapped", RenderGraph::TextureResourceType::RenderTarget,
			RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);

		m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	}

	// Request resources access
	{
		builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddBufferDependency(m_LuminanceHistogramBuffer, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_LuminanceAverageTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddTextureDependency(m_ToneMappedTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}
}

void st::gfx::ToneMappingRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	DeviceManager* deviceManager = GetDeviceManager();
	CommonResources* commonResources = deviceManager->GetCommonResources();
	UploadBuffer* uploadBuffer = deviceManager->GetUploadBuffer();
	rhi::Device* device = deviceManager->GetDevice();

	st::rhi::TextureHandle inputTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);
	st::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ToneMappedTexture);
	st::rhi::BufferHandle histogramBuffer = m_RenderGraph->GetBuffer(m_LuminanceHistogramBuffer);
	st::rhi::TextureHandle avgLuminanceTexture = m_RenderGraph->GetTexture(m_LuminanceAverageTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;
	assert(width == inputTexture->GetDesc().width);
	assert(height == inputTexture->GetDesc().height);
	
	if (!m_TonemappingEnabled)
	{
		commandList->SetPipelineState(commonResources->GetBlitComputePSO().get());
		
		interop::BlitComputeConstants shaderConstants;
		shaderConstants.srcTextureDI = inputTexture->GetSampledView();
		shaderConstants.dstTextureDI = outputTexture->GetStorageView();
		shaderConstants.viewBegin = float2{ 0, 0 };
		shaderConstants.viewEnd = float2{ width, height };
		commandList->PushComputeConstants(shaderConstants);

		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

		// Since we are not gonna do the tonemapping we have to do this transition here
		// to keep validation happy
		commandList->PushBarrier(
			rhi::Barrier::Texture(avgLuminanceTexture.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));
	}
	else
	{
		// Init stats buffer
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

			commandList->PushComputeConstants(shaderConstants);
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
			shaderConstants.viewEnd = uint2{ outputTexture->GetDesc().width, outputTexture->GetDesc().height };
			shaderConstants.minLogLuminance = m_MinLogLuminance;
			shaderConstants.oneOverLogLuminanceRange = 1.f / m_LogLuminanceRange;

			commandList->PushComputeConstants(shaderConstants);
			commandList->Dispatch((width + 15) / 16, (height + 15) / 16, 1);

			commandList->EndMarker();
		}

		// Average luminance
		{
			commandList->BeginMarker("Average luminance");

			commandList->PushBarriers({
				rhi::Barrier::Memory(histogramBuffer.get()),
				rhi::Barrier::Memory(m_StatsBuffer.get()) });

			commandList->SetPipelineState(m_AvgLuminancePSO.get());

			interop::AvgLuminanceHistogramConstants shaderConstants;
			shaderConstants.inputHistogramBufferDI = histogramBuffer->GetReadOnlyView();
			shaderConstants.outputAvgLuminanceTextureDI = avgLuminanceTexture->GetStorageView();
			shaderConstants.outputStatsBufferDI = m_StatsBuffer->GetReadWriteView();
			shaderConstants.pixelCount = width * height;
			shaderConstants.minLogLuminance = m_MinLogLuminance;
			shaderConstants.logLuminanceRange = m_LogLuminanceRange;
			shaderConstants.timeDelta = GetRenderView()->GetTimeDelta();
			shaderConstants.adaptionSpeedUp = m_AdaptationUpSpeed;
			shaderConstants.adaptionSpeedDown = m_AdaptationDownSpeed;

			commandList->PushComputeConstants(shaderConstants);
			commandList->Dispatch(1, 1, 1);

			commandList->EndMarker();
		}

		// Tone mapping
		switch (deviceManager->GetColorSpace())
		{
		case rhi::ColorSpace::SRGB:
			TonemapSDR(commandList);
			break;
		case rhi::ColorSpace::HDR10_ST2084:
			TonemapHDR(commandList);
			break;
		default:
			assert(0);
		}

		// Retrieve stats
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
}

void st::gfx::ToneMappingRenderStage::OnAttached()
{
	DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		
		m_BuildHistogramCS = shaderFactory->LoadShader("BuildLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_AvgLuminanceCS = shaderFactory->LoadShader("AvgLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_TonemappingSDR_CS = shaderFactory->LoadShader("ToneMappingSDR_cs", rhi::ShaderType::Compute);
		m_TonemappingHDR_CS = shaderFactory->LoadShader("ToneMappingHDR_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_BuildHistogramPSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BuildHistogramCS.get_weak() }, "BuildHistogramPSO");
		m_AvgLuminancePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_AvgLuminanceCS.get_weak() }, "AverageLuminancePSO");
		m_TonemappingSDR_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_TonemappingSDR_CS.get_weak() }, "TonemappingSDR_PSO");
		m_TonemappingHDR_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_TonemappingHDR_CS.get_weak() }, "TonemappingHDR_PSO");
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

void st::gfx::ToneMappingRenderStage::OnDetached()
{
	m_TonemappingHDR_PSO = nullptr;
	m_TonemappingHDR_CS = nullptr;

	m_TonemappingSDR_PSO = nullptr;
	m_TonemappingSDR_CS = nullptr;

	m_AvgLuminanceCS = nullptr;
	m_AvgLuminancePSO = nullptr;

	m_BuildHistogramPSO = nullptr;
	m_BuildHistogramCS = nullptr;
}

void st::gfx::ToneMappingRenderStage::TonemapHDR(st::rhi::CommandListHandle commandList)
{
	st::rhi::TextureHandle avgLuminanceTexture = m_RenderGraph->GetTexture(m_LuminanceAverageTexture);
	st::rhi::TextureHandle inputTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);
	st::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ToneMappedTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	commandList->BeginMarker("Tonemapping HDR");

	commandList->PushBarrier(
		rhi::Barrier::Texture(avgLuminanceTexture.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

	commandList->SetPipelineState(m_TonemappingHDR_PSO.get());

	interop::TonemapConstants shaderConstants = {};
	shaderConstants.inputTextureDI = inputTexture->GetSampledView();
	shaderConstants.inputAvgLuminanceTextureDI = avgLuminanceTexture->GetSampledView();
	shaderConstants.outputTextureDI = outputTexture->GetStorageView();
	shaderConstants.middleGray = m_SceneMiddleGray;
	shaderConstants.sdrExposureBias = m_sdrExposureBias;

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

	commandList->EndMarker();
}

void st::gfx::ToneMappingRenderStage::TonemapSDR(st::rhi::CommandListHandle commandList)
{
	st::rhi::TextureHandle avgLuminanceTexture = m_RenderGraph->GetTexture(m_LuminanceAverageTexture);
	st::rhi::TextureHandle inputTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);
	st::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ToneMappedTexture);
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	commandList->BeginMarker("Tonemapping SDR");

	commandList->PushBarrier(
		rhi::Barrier::Texture(avgLuminanceTexture.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

	commandList->SetPipelineState(m_TonemappingSDR_PSO.get());

	interop::TonemapConstants shaderConstants = {};
	shaderConstants.inputTextureDI = inputTexture->GetSampledView();
	shaderConstants.inputAvgLuminanceTextureDI = avgLuminanceTexture->GetSampledView();
	shaderConstants.outputTextureDI = outputTexture->GetStorageView();
	shaderConstants.middleGray = m_SceneMiddleGray;
	shaderConstants.sdrExposureBias = m_sdrExposureBias;

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

	commandList->EndMarker();
}
