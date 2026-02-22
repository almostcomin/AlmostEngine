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
	if (!m_RenderView->GetScene() || !m_SSAOEnabled)
	{
		Passthrough();
		return;
	}

	auto camera = m_RenderView->GetCamera();
	const float4x4& projMatrix = camera->GetProjectionMatrix();

	rhi::TextureHandle linearDepthTex = m_RenderView->GetTexture("LinearDepth");
	rhi::TextureHandle AOTex = m_RenderView->GetTexture("AmbientOcclusion");
	rhi::TextureHandle blurTexTemp = m_RenderView->GetTexture("AOBlurTemp");

	auto AOTexSampled = AOTex->GetSampledView();
	auto AOTexStorage = AOTex->GetStorageView();
	auto blurTempSampled = blurTexTemp->GetSampledView();
	auto blurTempStorage = blurTexTemp->GetStorageView();
	auto linearDepthSampled = linearDepthTex->GetSampledView();
	const uint32_t width = linearDepthTex->GetDesc().width;
	const uint32_t height = linearDepthTex->GetDesc().height;

	rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

	//
	// SSAO
	//

	commandList->BeginMarker("SSAO");
	{
		commandList->SetPipelineState(m_SSAO_PSO.get());

		interop::SSAOConstants shaderConstants;
		shaderConstants.sceneDI = m_RenderView->GetSceneBufferUniformView();
		shaderConstants.depthTextureDI = linearDepthSampled;
		shaderConstants.normalsTextureDI = m_RenderView->GetTextureSampledView("GBuffer2");
		shaderConstants.outputAOTextureDI = AOTexStorage;
		shaderConstants.textureWidth = width;
		shaderConstants.textureHeight = height;

		shaderConstants.radiusWorld = m_Radius;
		shaderConstants.power = m_Power;
		shaderConstants.surfaceBias = m_Bias;

		shaderConstants.clipToWindowScale = float2(0.5f * width, -0.5f * height);
		shaderConstants.clipToWindowBias = float2(width * 0.5f, height * 0.5f);
		shaderConstants.windowToClipScale = 1.f / shaderConstants.clipToWindowScale;
		shaderConstants.windowToClipBias = -shaderConstants.clipToWindowBias * shaderConstants.windowToClipScale;
		shaderConstants.clipToView = float2(
			1.f / projMatrix[0].x,
			1.f / projMatrix[1].y);
		shaderConstants.invBackgroundViewDepth = 1.f / 1000;
		shaderConstants.radiusToScreen = 0.5f * height * abs(projMatrix[1].y);

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
		commandList->EndMarker();
	}

	//
	// Blur bilateral - Vertical pass
	//

	commandList->BeginMarker("BilaterialBlur - Vertical pass");
	{
		commandList->SetPipelineState(m_BilateralBlurV_PSO.get());

		commandList->PushBarrier(
			rhi::Barrier::Texture(AOTex.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

		interop::BilaterialBlurConstants shaderConstants;
		shaderConstants.inputTextureDI = AOTexSampled;
		shaderConstants.depthTextureDI = linearDepthSampled;
		shaderConstants.outputTextureDI = blurTempStorage;
		shaderConstants.textureWidth = width;
		shaderConstants.textureHeight = height;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch(DivRoundUp(height, 128u), width, 1);

		commandList->EndMarker();
	}

	//
	// Blur bilateral - Horizontal pass
	//

	commandList->BeginMarker("BilaterialBlur - Horizontal pass");
	{
		commandList->SetPipelineState(m_BilateralBlurH_PSO.get());

		commandList->PushBarriers({
			rhi::Barrier::Texture(AOTex.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS),
			rhi::Barrier::Texture(blurTexTemp.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE) });

		interop::BilaterialBlurConstants shaderConstants;
		shaderConstants.inputTextureDI = blurTempSampled;
		shaderConstants.depthTextureDI = linearDepthSampled;
		shaderConstants.outputTextureDI = AOTexStorage;
		shaderConstants.textureWidth = width;
		shaderConstants.textureHeight = height;

		commandList->PushComputeConstants(shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 128u), height, 1);

		commandList->EndMarker();
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
		m_RenderView->CreateTexture("AOBlurTemp", RenderView::TextureResourceType::ShaderResource,
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
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "AOBlurTemp",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_SSAO_CS = shaderFactory->LoadShader("SSAO_cs", rhi::ShaderType::Compute);
		m_BilateralBlurV_CS = shaderFactory->LoadShader("BilateralBlurV_cs", rhi::ShaderType::Compute);
		m_BilateralBlurH_CS = shaderFactory->LoadShader("BilateralBlurH_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_SSAO_PSO = device->CreateComputePipelineState({ m_SSAO_CS.get_weak() }, "SSAO_PSO");
		m_BilateralBlurV_PSO = device->CreateComputePipelineState({ m_BilateralBlurV_CS.get_weak() }, "BilateralBlurV_CS");
		m_BilateralBlurH_PSO = device->CreateComputePipelineState({ m_BilateralBlurH_CS.get_weak() }, "BilateralBlurH_CS");
	}
}

void st::gfx::SSAORenderStage::OnDetached()
{
	m_BilateralBlurH_PSO.reset();
	m_BilateralBlurV_PSO.reset();
	m_SSAO_CS.reset();

	m_BilateralBlurH_CS.reset();
	m_BilateralBlurV_CS.reset();
	m_SSAO_PSO.reset();

}

void st::gfx::SSAORenderStage::Passthrough()
{
	rhi::TextureHandle linearDepthTex = m_RenderView->GetTexture("LinearDepth");
	const uint32_t width = linearDepthTex->GetDesc().width;
	const uint32_t height = linearDepthTex->GetDesc().height;

	rhi::CommandListHandle commandList = m_RenderView->GetCommandList();
	auto* commonResources = m_RenderView->GetDeviceManager()->GetCommonResources();

	commandList->SetPipelineState(commonResources->GetClearTexturePSO().get());

	// Transition temp texture to keep validation happy
	commandList->PushBarrier(
		rhi::Barrier::Texture(m_RenderView->GetTexture("AOBlurTemp").get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

	interop::ClearTextureConstants shaderConstants;
	shaderConstants.textureDI = m_RenderView->GetTextureStorageView("AmbientOcclusion");
	shaderConstants.textureDim = float2{ width, height };
	shaderConstants.clearValue = float4{ 1.f };

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
}
