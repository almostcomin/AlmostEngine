#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void st::gfx::ToneMappingRenderStage::Render()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	st::rhi::TextureHandle inputTexture = m_RenderView->GetTexture("SceneColor");
	st::rhi::TextureHandle outputTexture = m_RenderView->GetTexture("ToneMapped");
	st::rhi::BufferHandle histogramBuffer = m_RenderView->GetBuffer("LuminanceHistogram");
	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;
	assert(width == inputTexture->GetDesc().width);
	assert(height == inputTexture->GetDesc().height);

	// First pass, build luminance histogram
	{
		commandList->BeginMarker("Build histogram");		
		commandList->SetPipelineState(m_BuildHistogramPSO.get());

		interop::BuildLuminanceHistogramConstants shaderConstants;
		shaderConstants.inputTextureDI = inputTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
		shaderConstants.outputHistogramBufferDI = histogramBuffer->GetShaderViewIndex(rhi::BufferShaderView::UnorderedAccess);
		shaderConstants.viewBegin = uint2{ 0 };
		shaderConstants.viewEnd = uint2{ outputTexture->GetDesc().width, outputTexture->GetDesc().height };
		shaderConstants.minLogLuminance = -10.f;
		shaderConstants.oneOverLogLuminanceRange = 1.f / 12;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch((width + 15)/ 16, (height + 15) / 16, 1);

		commandList->EndMarker();
	}


	commandList->SetPipelineState(m_PSO.get());

	interop::TonemapConstants shaderConstants = {};
	shaderConstants.inputTextureDI = inputTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
	shaderConstants.outputTextureDI = outputTexture->GetShaderViewIndex(rhi::TextureShaderView::UnorderedAcces);
	shaderConstants.exposure = m_Exposure;
	shaderConstants.tonemapping = (uint)m_Tonemapping;

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);	
}

void st::gfx::ToneMappingRenderStage::OnAttached()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create resources
	{
		m_RenderView->CreateTexture("ToneMapped", RenderView::TextureResourceType::RenderTarget,
			RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);
		m_RenderView->CreateBuffer("LuminanceHistogram", rhi::BufferDesc{
			.shaderUsage = rhi::BufferShaderUsage::ShaderResource | rhi::BufferShaderUsage::UnorderedAccess,
			.sizeBytes = 256 * sizeof(uint32_t) });
	}

	// Request resources access
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneColor",
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestBufferAccess(this, RenderView::AccessMode::Write, "LuminanceHistogram",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ToneMapped",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_BuildHistogramCS = shaderFactory->LoadShader("BuildLuminanceHistogram_cs", rhi::ShaderType::Compute);
		m_CS = shaderFactory->LoadShader("ToneMapping_cs", rhi::ShaderType::Compute);
	}

	// Create PSO
	{
		m_BuildHistogramPSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BuildHistogramCS.get_weak() }, "BuildHistogramPSO");
		m_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_CS.get_weak() }, "TonemappingPSO");
	}
}

void st::gfx::ToneMappingRenderStage::OnDetached()
{
	m_PSO = nullptr;
	m_CS = nullptr;

	m_BuildHistogramPSO = nullptr;
	m_BuildHistogramCS = nullptr;
}