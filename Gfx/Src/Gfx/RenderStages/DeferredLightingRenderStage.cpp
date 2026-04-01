#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/RenderView.h"

void alm::gfx::DeferredLightingRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.CreateColorTarget("SceneColor", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);

	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");
	m_ShadowmapTexture = builder.GetTextureHandle("Shadowmap");
	m_GBuffer0Texture = builder.GetTextureHandle("GBuffer0");
	m_GBuffer1Texture = builder.GetTextureHandle("GBuffer1");
	m_GBuffer2Texture = builder.GetTextureHandle("GBuffer2");
	m_GBuffer3Texture = builder.GetTextureHandle("GBuffer3");
	m_AmbientOcclusionTexture = builder.GetTextureHandle("AmbientOcclusion");

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);

	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_ShadowmapTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_GBuffer0Texture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_GBuffer1Texture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_GBuffer2Texture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_GBuffer3Texture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_AmbientOcclusionTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::DeferredLightingRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	rhi::Device* device = GetDeviceManager()->GetDevice();
	auto shadowmapTex = m_RenderGraph->GetTexture(m_ShadowmapTexture);
	float2 shadowMapResolution = { 0.f, 0.f };
	rhi::TextureSampledView shadowmapSampled = {};
	if (shadowmapTex)
	{
		shadowMapResolution = { (float)shadowmapTex->GetDesc().width, (float)shadowmapTex->GetDesc().height };
		shadowmapSampled = shadowmapTex->GetSampledView();
	}

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() }},
		{},
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));

	interop::DeferredLightingConstants shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.sceneDepthDI = m_RenderGraph->GetTextureSampledView(m_SceneDepthTexture);
	shaderConstants.shadowMapDI = shadowmapSampled;
	shaderConstants.GBuffer0DI = m_RenderGraph->GetTextureSampledView(m_GBuffer0Texture);
	shaderConstants.GBuffer1DI = m_RenderGraph->GetTextureSampledView(m_GBuffer1Texture);
	shaderConstants.GBuffer2DI = m_RenderGraph->GetTextureSampledView(m_GBuffer2Texture);
	shaderConstants.GBuffer3DI = m_RenderGraph->GetTextureSampledView(m_GBuffer3Texture);
	shaderConstants.SSAO_DI = m_RenderGraph->GetTextureSampledView(m_AmbientOcclusionTexture);
	shaderConstants.oneOverShadowmapResolution = 1.f / shadowMapResolution;
	shaderConstants.MaterialChannel = (uint)m_MaterialChannel;
	shaderConstants.ShowSSAO = m_ShowSSAO;
	shaderConstants.ShowShadowmap = m_ShowShadowmap;

	commandList->PushGraphicsConstants(0, shaderConstants);
	commandList->Draw(3);

	commandList->EndRenderPass();
}

void alm::gfx::DeferredLightingRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	alm::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "DeferredBaseRenderStage");
	}

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("DeferredLighting_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		m_PSO = commonResources->CreateFullscreenPassPSO(
			m_FB->GetFramebufferInfo(), m_PS.get_weak(), "DeferredLightingRenderStage");
	}
}

void alm::gfx::DeferredLightingRenderStage::OnDetached()
{
	alm::rhi::Device* device = GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PSO));
	device->ReleaseQueued(std::move(m_PS));
}

void alm::gfx::DeferredLightingRenderStage::OnBackbufferResize()
{
	alm::gfx::CommonResources* commonResources = GetDeviceManager()->GetCommonResources();
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "DeferredLightingRenderStage");
	}

	// Re-create PSO
	m_PSO = commonResources->CreateFullscreenPassPSO(
		m_FB->GetFramebufferInfo(), m_PS.get_weak(), "DeferredLightingRenderStage");
}