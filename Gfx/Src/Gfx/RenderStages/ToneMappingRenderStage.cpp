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

	m_StatsBufferReadBack->Unmap();

	st::rhi::TextureHandle outputTexture = m_RenderView->GetTexture("ToneMapped");
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	result.totalPixels = width * height;

	return result;
}

void st::gfx::ToneMappingRenderStage::Render()
{
	CommonResources* commonResources = m_RenderView->GetDeviceManager()->GetCommonResources();
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	UploadBuffer* uploadBuffer = deviceManager->GetUploadBuffer();
	rhi::Device* device = deviceManager->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	st::rhi::TextureHandle inputTexture = m_RenderView->GetTexture("SceneColor");
	st::rhi::TextureHandle outputTexture = m_RenderView->GetTexture("ToneMapped");
	st::rhi::BufferHandle histogramBuffer = m_RenderView->GetBuffer("LuminanceHistogram");
	st::rhi::TextureHandle avgLuminanceTexture = m_RenderView->GetTexture("LuminanceAverage");
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;
	assert(width == inputTexture->GetDesc().width);
	assert(height == inputTexture->GetDesc().height);
	
	// Init stats buffer
	{
		commandList->BeginMarker("Init stats");

		auto [initialStats, offset] = uploadBuffer->RequestSpaceForBufferDataUpload<interop::TonemappingStatsBuffer>();

		initialStats->minLuminance = FLT_MAX;
		initialStats->maxLuminance = 0.0f;

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
		shaderConstants.bufferDI = histogramBuffer->GetShaderViewIndex(rhi::BufferShaderView::UnorderedAccess);
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
		shaderConstants.inputTextureDI = inputTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
		shaderConstants.outputHistogramBufferDI = histogramBuffer->GetShaderViewIndex(rhi::BufferShaderView::UnorderedAccess);
		shaderConstants.outputStatsBufferDI = m_StatsBuffer->GetShaderViewIndex(rhi::BufferShaderView::UnorderedAccess);
		shaderConstants.viewBegin = uint2{ 0 };
		shaderConstants.viewEnd = uint2{ outputTexture->GetDesc().width, outputTexture->GetDesc().height };
		shaderConstants.minLogLuminance = m_MinLogLuminance;
		shaderConstants.oneOverLogLuminanceRange = 1.f / m_LogLuminanceRange;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch((width + 15)/ 16, (height + 15) / 16, 1);

		commandList->EndMarker();
	}

	// Average luminance
	{
		commandList->BeginMarker("Average luminance");
		commandList->PushBarrier(rhi::Barrier::Memory(histogramBuffer.get()));
		commandList->SetPipelineState(m_AvgLuminancePSO.get());

		interop::AvgLuminanceHistogramConstants shaderConstants;
		shaderConstants.inputHistogramBufferDI = histogramBuffer->GetShaderViewIndex(rhi::BufferShaderView::UnorderedAccess);
		shaderConstants.outputAvgLuminanceTextureDI = avgLuminanceTexture->GetShaderViewIndex(rhi::TextureShaderView::UnorderedAccess);
		shaderConstants.pixelCount = width * height;
		shaderConstants.minLogLuminance = m_MinLogLuminance;
		shaderConstants.logLuminanceRange = m_LogLuminanceRange;
		shaderConstants.timeDelta = m_RenderView->GetTimeDelta();
		shaderConstants.tau = 1.1f;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch(1, 1, 1);

		commandList->EndMarker();
	}

	// Tone mapping
	{
		commandList->BeginMarker("Tonemapping");

		commandList->PushBarrier(
			rhi::Barrier::Texture(avgLuminanceTexture.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

		commandList->SetPipelineState(m_PSO.get());

		interop::TonemapConstants shaderConstants = {};
		shaderConstants.inputTextureDI = inputTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
		shaderConstants.inputAvgLuminanceTextureDI = avgLuminanceTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
		shaderConstants.outputTextureDI = outputTexture->GetShaderViewIndex(rhi::TextureShaderView::UnorderedAccess);
		shaderConstants.exposure = m_Exposure;
		shaderConstants.tonemapping = (uint)m_Tonemapping;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

		commandList->EndMarker();
	}

	// Retrieve stats
	{
		commandList->PushBarrier(rhi::Barrier::Memory(m_StatsBuffer.get()));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::COPY_SRC));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBufferReadBack.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST));
		
		commandList->CopyBufferToBuffer(m_StatsBufferReadBack.get(), 0, m_StatsBuffer.get(), 0, sizeof(interop::TonemappingStatsBuffer));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBufferReadBack.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::COMMON));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COPY_SRC, rhi::ResourceState::COMMON));
	}
}

void st::gfx::ToneMappingRenderStage::OnAttached()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create resources
	{
		m_RenderView->CreateBuffer("LuminanceHistogram", rhi::BufferDesc{
			.shaderUsage = rhi::BufferShaderUsage::ShaderResource | rhi::BufferShaderUsage::UnorderedAccess,
			.sizeBytes = 256 * sizeof(uint32_t) });

		m_RenderView->CreateTexture("LuminanceAverage", RenderView::TextureResourceType::ShaderResource,
			1, 1, 1, rhi::Format::R32_FLOAT, true);

		m_RenderView->CreateTexture("ToneMapped", RenderView::TextureResourceType::RenderTarget,
			RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);
	}

	// Request resources access
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneColor",
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestBufferAccess(this, RenderView::AccessMode::Write, "LuminanceHistogram",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "LuminanceAverage",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ToneMapped",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		
		m_BuildHistogramCS = shaderFactory->LoadShader("BuildLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_AvgLuminanceCS = shaderFactory->LoadShader("AvgLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_CS = shaderFactory->LoadShader("ToneMapping_cs", rhi::ShaderType::Compute);
	}

	// Create PSO
	{
		m_BuildHistogramPSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BuildHistogramCS.get_weak() }, "BuildHistogramPSO");
		m_AvgLuminancePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_AvgLuminanceCS.get_weak() }, "AverageLuminancePSO");
		m_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_CS.get_weak() }, "TonemappingPSO");
	}

	// Stats
	{
		{
			auto desc = rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::UnorderedAccess,
				.sizeBytes = sizeof(interop::TonemappingStatsBuffer),
				.format = rhi::Format::UNKNOWN,
				.stride = 0 };

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
	m_PSO = nullptr;
	m_CS = nullptr;

	m_AvgLuminanceCS = nullptr;
	m_AvgLuminancePSO = nullptr;

	m_BuildHistogramPSO = nullptr;
	m_BuildHistogramCS = nullptr;
}