#include "Gfx/RenderStages/WBOITResolveRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderGraph.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void st::gfx::WBOITResolveRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_AccumWOITTexture = builder.GetTextureHandle("AccumWOIT");
	m_RevealageWOITTexture = builder.GetTextureHandle("RevealageWOIT");
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");

	builder.AddTextureDependency(m_AccumWOITTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_RevealageWOITTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
}

void st::gfx::WBOITResolveRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store }},
		{},
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::WBOITResolveStageConstants shaderConstants;
	shaderConstants.accumDI = m_RenderGraph->GetTextureSampledView(m_AccumWOITTexture);
	shaderConstants.revealageDI = m_RenderGraph->GetTextureSampledView(m_RevealageWOITTexture);

	commandList->PushGraphicsConstants(0, shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void st::gfx::WBOITResolveRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = GetDeviceManager();
	st::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "WBOITResolveRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("WBOITResolve_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
		{
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::SrcAlpha,
			.destBlend = rhi::BlendFactor::OneMinusSrcAlpha,
			.blendOp = rhi::BlendOp::Add
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
			.rasterState = rasterState,
		};

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "WBOITResolveRenderStage");
	}
}

void st::gfx::WBOITResolveRenderStage::OnDetached()
{
	m_PSO.reset();
	m_PS.reset();
	m_FB.reset();
}

void st::gfx::WBOITResolveRenderStage::OnBackbufferResize()
{
	st::gfx::DeviceManager* deviceManager = GetDeviceManager();
	st::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_SceneColorTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "WBOITResolveRenderStage");
	}

	// Create PSO
	{
		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "WBOITResolveRenderStage");
	}
}