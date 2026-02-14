#include "Gfx/RenderStages/SSAORenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/DataUploader.h"
#include "Gfx/CommonResources.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void st::gfx::SSAORenderStage::Render()
{
	if (!m_RenderView->GetScene())
		return;
	auto camera = m_RenderView->GetCamera();

	auto linearDepthTex = m_RenderView->GetTexture("LinearDepth");
	const uint32_t width = linearDepthTex->GetDesc().width;
	const uint32_t height = linearDepthTex->GetDesc().height;

	if (m_SSAOEnabled)
	{
		const uint32_t noiseWidth = m_NoiseTexture->GetDesc().width;
		const uint32_t noiseHeight = m_NoiseTexture->GetDesc().height;

		rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

		commandList->SetPipelineState(m_SSAO_PSO.get());

		interop::SSAOConstants shaderConstants;
		shaderConstants.sceneDI = m_RenderView->GetSceneBufferUniformView();
		shaderConstants.depthTextureDI = m_RenderView->GetTextureSampledView("LinearDepth");
		shaderConstants.normalsTextureDI = m_RenderView->GetTextureSampledView("GBuffer2");
		shaderConstants.noiseTextureDI = m_NoiseTexture->GetSampledView();
		shaderConstants.outputAOTextureDI = m_RenderView->GetTextureStorageView("AmbientOcclusion");
		shaderConstants.textureWidth = width;
		shaderConstants.textureHeight = height;
		shaderConstants.noiseScale = { (float)width / noiseWidth, (float)height / noiseHeight };
		shaderConstants.radius = 0.01f;
		shaderConstants.power = 2.f;
		shaderConstants.bias = 0.005f;
		shaderConstants.frustumTopLeft = float4{ camera->GetFrustumTopLeftDirView(), 0.f };
		shaderConstants.frustumVecX = float4{ camera->GetFrustumTopRightDirView() - camera->GetFrustumTopLeftDirView(), 0.f };
		shaderConstants.frustumVecY = float4{ camera->GetFrustumBottomLeftDirView() - camera->GetFrustumTopLeftDirView(), 0.f };

		commandList->PushComputeConstants(shaderConstants);

		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
	}
	else
	{
		auto* commonResources = m_RenderView->GetDeviceManager()->GetCommonResources();
		rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

		commandList->SetPipelineState(commonResources->GetClearTexturePSO().get());

		interop::ClearTextureConstants shaderConstants;
		shaderConstants.textureDI = m_RenderView->GetTextureStorageView("AmbientOcclusion");
		shaderConstants.textureDim = float2{ width, height };
		shaderConstants.clearValue = float4{ 1.f };

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
	}
}

void st::gfx::SSAORenderStage::OnAttached()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create resources
	{
		m_RenderView->CreateTexture("AmbientOcclusion", RenderView::TextureResourceType::ShaderResource,
			st::gfx::RenderView::c_BBSize, st::gfx::RenderView::c_BBSize, 1, rhi::Format::R16_FLOAT, true);
	}

	// Declare resource access
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "LinearDepth",
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "GBuffer2",
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "AmbientOcclusion",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_SSAO_CS = shaderFactory->LoadShader("SSAO_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_SSAO_PSO = device->CreateComputePipelineState({ m_SSAO_CS.get_weak() }, "SSAO_PSO");
	}

	CreateNoiseTexture();
}

void st::gfx::SSAORenderStage::OnDetached()
{
	m_NoiseTexture.reset();
	m_SSAO_PSO.reset();
	m_SSAO_CS.reset();
}

void st::gfx::SSAORenderStage::CreateNoiseTexture()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();
	DataUploader* dataUploader = deviceManager->GetDataUploader();

	// Create the texture
	rhi::TextureDesc desc{
		.width = 8,
		.height = 8,
		.format = rhi::Format::RG16_FLOAT,
		.shaderUsage = rhi::TextureShaderUsage::Sampled };
	m_NoiseTexture = device->CreateTexture(desc, rhi::ResourceState::COPY_DST, "SSAONoiseTexture");

	// Upload data
	auto uploadTicket = dataUploader->RequestUploadTicket(desc);
	auto* data = (uint16_t*)uploadTicket->GetPtr();
	
	std::mt19937 gen(47);
	std::uniform_real_distribution<float> dist(0.f, 1.f);

	rhi::SubresourceCopyableRequirements req = device->GetSubresourceCopyableRequirements(desc, 0, 0);
	assert(req.numRows == desc.height);

	for (int row = 0; row < desc.height; ++row)
	{
		for (int col = 0; col < desc.width; ++col)
		{
			float angle = dist(gen) * 2.f * PI;
			data[col * 2 + 0] = FloatToHalf(cosf(angle));
			data[col * 2 + 1] = FloatToHalf(sinf(angle));
		}
		data = (uint16_t*)(((char*)data) + req.rowStride);
	}

	auto commitResult = dataUploader->CommitUploadTextureTicket(std::move(*uploadTicket), m_NoiseTexture.get_weak(),
		rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
	assert(commitResult.has_value());
	
	commitResult->Wait();
}