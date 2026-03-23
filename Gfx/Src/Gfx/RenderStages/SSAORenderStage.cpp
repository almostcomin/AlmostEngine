#include "Gfx/RenderStages/SSAORenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/DataUploader.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void alm::gfx::SSAORenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create resources
	{
		m_AmbientOcclusionTexture = builder.CreateTexture("AmbientOcclusion", RenderGraph::TextureResourceType::ShaderResource, 
			alm::gfx::RenderGraph::c_BBSize, alm::gfx::RenderGraph::c_BBSize, 1, rhi::Format::R16_FLOAT, true);
		m_AOBlurTempTexture = builder.CreateTexture("AOBlurTemp", RenderGraph::TextureResourceType::ShaderResource, 
			alm::gfx::RenderGraph::c_BBSize, alm::gfx::RenderGraph::c_BBSize, 1, rhi::Format::R16_FLOAT, true);

		m_LinearDepthTexture = builder.GetTextureHandle("LinearDepth");
		m_GBuffer2Texture = builder.GetTextureHandle("GBuffer2");
	}

	// Declare resource access
	{
		builder.AddTextureDependency(m_LinearDepthTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddTextureDependency(m_GBuffer2Texture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddTextureDependency(m_AmbientOcclusionTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
		builder.AddTextureDependency(m_AOBlurTempTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE);
	}
}

void alm::gfx::SSAORenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!GetScene() || !m_SSAOEnabled)
	{
		Passthrough(commandList);
		return;
	}

	auto camera = GetRenderView()->GetCamera();
	const float4x4& projMatrix = camera->GetProjectionMatrix();

	rhi::TextureHandle linearDepthTex = m_RenderGraph->GetTexture(m_LinearDepthTexture);
	rhi::TextureHandle AOTex = m_RenderGraph->GetTexture(m_AmbientOcclusionTexture);
	rhi::TextureHandle blurTexTemp = m_RenderGraph->GetTexture(m_AOBlurTempTexture);

	auto AOTexSampled = AOTex->GetSampledView();
	auto AOTexStorage = AOTex->GetStorageView();
	auto blurTempSampled = blurTexTemp->GetSampledView();
	auto blurTempStorage = blurTexTemp->GetStorageView();
	auto linearDepthSampled = linearDepthTex->GetSampledView();
	const uint32_t width = linearDepthTex->GetDesc().width;
	const uint32_t height = linearDepthTex->GetDesc().height;

	//
	// SSAO
	//

	commandList->BeginMarker("SSAO");
	{
		commandList->SetPipelineState(m_SSAO_PSO.get());

		interop::SSAOConstants shaderConstants;
		shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
		shaderConstants.depthTextureDI = linearDepthSampled;
		shaderConstants.normalsTextureDI = m_RenderGraph->GetTextureSampledView(m_GBuffer2Texture);
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

		commandList->PushComputeConstants(0, shaderConstants);
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

		commandList->PushComputeConstants(0, shaderConstants);
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

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(DivRoundUp(width, 128u), height, 1);

		commandList->EndMarker();
	}
}

void alm::gfx::SSAORenderStage::OnAttached()
{
	DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
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

void alm::gfx::SSAORenderStage::OnDetached()
{
	m_BilateralBlurH_PSO.reset();
	m_BilateralBlurV_PSO.reset();
	m_SSAO_CS.reset();

	m_BilateralBlurH_CS.reset();
	m_BilateralBlurV_CS.reset();
	m_SSAO_PSO.reset();

}

void alm::gfx::SSAORenderStage::Passthrough(alm::rhi::CommandListHandle commandList)
{
	rhi::TextureHandle linearDepthTex = m_RenderGraph->GetTexture(m_LinearDepthTexture);
	const uint32_t width = linearDepthTex->GetDesc().width;
	const uint32_t height = linearDepthTex->GetDesc().height;

	auto* commonResources = GetDeviceManager()->GetCommonResources();

	commandList->SetPipelineState(commonResources->GetClearTexturePSO().get());

	// Transition temp texture to keep validation happy
	commandList->PushBarrier(rhi::Barrier::Texture(
		m_RenderGraph->GetTexture(m_AOBlurTempTexture).get(),rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

	interop::ClearTextureConstants shaderConstants;
	shaderConstants.textureDI = m_RenderGraph->GetTextureStorageView(m_AmbientOcclusionTexture);
	shaderConstants.textureDim = float2{ width, height };
	shaderConstants.clearValue = float4{ 1.f };

	commandList->PushComputeConstants(0, shaderConstants);
	commandList->Dispatch(DivRoundUp(width, 16u), DivRoundUp(height, 16u), 1);
}
