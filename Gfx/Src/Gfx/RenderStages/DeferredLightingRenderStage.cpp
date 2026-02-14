#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"

void st::gfx::DeferredLightingRenderStage::Render()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

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
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferUniformView();
	shaderConstants.sceneDepthDI = m_RenderView->GetTextureSampledView("SceneDepth");
	shaderConstants.shadowMapDI = m_RenderView->GetTextureSampledView("Shadowmap");
	shaderConstants.GBuffer0DI = m_RenderView->GetTextureSampledView("GBuffer0");
	shaderConstants.GBuffer1DI = m_RenderView->GetTextureSampledView("GBuffer1");
	shaderConstants.GBuffer2DI = m_RenderView->GetTextureSampledView("GBuffer2");
	shaderConstants.GBuffer3DI = m_RenderView->GetTextureSampledView("GBuffer3");
	shaderConstants.SSAO_DI = m_RenderView->GetTextureSampledView("AmbientOcclusion");

	commandList->PushGraphicsConstants(shaderConstants);
	commandList->Draw(3);

	commandList->EndRenderPass();
}

void st::gfx::DeferredLightingRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	st::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create render target
	m_RenderView->CreateColorTarget("SceneColor", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);

	// Request access
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "Shadowmap", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "GBuffer0", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "GBuffer1", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "GBuffer2", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "GBuffer3", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "AmbientOcclusion", rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("SceneColor"));
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
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PSO));
	device->ReleaseQueued(std::move(m_PS));
}

void st::gfx::DeferredLightingRenderStage::OnBackbufferResize()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("SceneColor"));
		m_FB = device->CreateFramebuffer(fbDesc, "DeferredLightingRenderStage");
	}

	// Re-create PSO
	m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "DeferredLightingRenderStage");
}