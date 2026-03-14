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

void st::gfx::DeferredLightingRenderStage::Setup(RenderGraphBuilder& builder)
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

void st::gfx::DeferredLightingRenderStage::Render(st::rhi::CommandListHandle commandList)
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

	commandList->PushGraphicsConstants(shaderConstants);
	commandList->Draw(3);

	commandList->EndRenderPass();
}

void st::gfx::DeferredLightingRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = GetDeviceManager();
	st::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "DeferredBaseRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("DeferredLighting_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
		{
			.blendEnable = false,
		};

		rhi::RasterizerState rasterState =
		{
			.fillMode = rhi::FillMode::Solid,
			.cullMode = rhi::CullMode::None
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilEnable = false
		};

		m_PSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = commonResources->GetBlitVS(),
			.PS = m_PS.get_weak(),
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "DeferredLightingRenderStage");
	}
}

void st::gfx::DeferredLightingRenderStage::OnDetached()
{
	st::rhi::Device* device = GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PSO));
	device->ReleaseQueued(std::move(m_PS));
}

void st::gfx::DeferredLightingRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "DeferredLightingRenderStage");
	}

	// Re-create PSO
	m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "DeferredLightingRenderStage");
}