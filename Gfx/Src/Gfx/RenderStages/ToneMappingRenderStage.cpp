#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

alm::gfx::ToneMappingRenderStage::ToneMappingRenderStage()
{}

void alm::gfx::ToneMappingRenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create resources
	{
		m_ToneMappedTexture = builder.CreateTexture("ToneMapped", RenderGraph::TextureResourceType::RenderTarget,
			1, RenderGraph::SizeSpace::SceneViewport, 1, rhi::Format::RGBA16_FLOAT, true);
		m_BloomResultTexture = builder.GetTextureHandle("BloomResult");
		m_ExposureRatioTexture = builder.GetTextureHandle("ExposureRatio");
	}

	// Request resources access
	{
		builder.AddTextureDependency(m_BloomResultTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddTextureDependency(m_ToneMappedTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_ExposureRatioTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	}
}

void alm::gfx::ToneMappingRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	DeviceManager* deviceManager = GetDeviceManager();
	CommonResources* commonResources = deviceManager->GetCommonResources();

	alm::rhi::TextureHandle inputTexture = m_RenderGraph->GetTexture(m_BloomResultTexture);
	alm::rhi::TextureHandle exposureRatioTexture = m_RenderGraph->GetTexture(m_ExposureRatioTexture);
	alm::rhi::TextureHandle outputTexture = m_RenderGraph->GetTexture(m_ToneMappedTexture);
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
		commandList->PushComputeConstants(0, shaderConstants);

		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
	}
	else
	{
		switch (deviceManager->GetColorSpace())
		{
		case rhi::ColorSpace::SRGB:
			commandList->BeginMarker("Tonemapping SDR");
			commandList->SetPipelineState(m_TonemappingSDR_PSO.get());
			break;
		case rhi::ColorSpace::HDR10_ST2084:
			commandList->BeginMarker("Tonemapping HDR");
			commandList->SetPipelineState(m_TonemappingHDR_PSO.get());
			break;
		default:
			assert(0);
		}

		interop::TonemapConstants shaderConstants;
		shaderConstants.InputColorTextureDI = inputTexture->GetSampledView();
		shaderConstants.InputExposureRatioTextureDI = exposureRatioTexture->GetSampledView();
		shaderConstants.OutputTextureDI = outputTexture->GetStorageView();
		shaderConstants.TextureDims = uint2{ width, height };

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);

		commandList->EndMarker();
	}
}

void alm::gfx::ToneMappingRenderStage::OnAttached()
{
	DeviceManager* deviceManager = GetDeviceManager();
	alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{		
		m_TonemappingSDR_CS = shaderFactory->LoadShader("ToneMappingSDR_cs", rhi::ShaderType::Compute);
		m_TonemappingHDR_CS = shaderFactory->LoadShader("ToneMappingHDR_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_TonemappingSDR_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_TonemappingSDR_CS.get_weak() }, "TonemappingSDR_PSO");
		m_TonemappingHDR_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_TonemappingHDR_CS.get_weak() }, "TonemappingHDR_PSO");
	}
}

void alm::gfx::ToneMappingRenderStage::OnDetached()
{
	m_TonemappingHDR_PSO = nullptr;
	m_TonemappingHDR_CS = nullptr;

	m_TonemappingSDR_PSO = nullptr;
	m_TonemappingSDR_CS = nullptr;
}
