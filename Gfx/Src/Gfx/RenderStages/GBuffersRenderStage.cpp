#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/RenderHelpers.h"
#include "Gfx/RenderGraphBuilder.h"

void st::gfx::GBuffersRenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create render targets
	{
		// GBuffer0
		//	RGB = Base color (albedo)
		//	A = Opacity
		m_GBuffer0Texture = builder.CreateColorTarget("GBuffer0", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		// GBuffer1
		//	RGB = SpecularF0
		//	A = Occlusion
		m_GBuffer1Texture = builder.CreateColorTarget("GBuffer1", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		// GBuffer2
		//	RG = Material (encoded)
		//  B = Roughness
		//  A = Metalness
		m_GBuffer2Texture = builder.CreateColorTarget("GBuffer2", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);

		// GBuffer3
		//	RGB = Emissive color
		//	A = unused
		m_GBuffer3Texture = builder.CreateColorTarget("GBuffer3", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");
	}

	// Request RT access
	builder.AddTextureDependency(m_GBuffer0Texture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_GBuffer1Texture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_GBuffer2Texture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_GBuffer3Texture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void st::gfx::GBuffersRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		return;
	}

	commandList->BeginRenderPass(
		m_FB.get(),
		{ 
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() }
		},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() },
		{},
		rhi::RenderPassFlags::None);

	RenderSetInstanced(
		GetRenderView()->GetCameraVisibleSet(),
		GetRenderView()->GetSceneBufferUniformView(),
		GetRenderView()->GetCameraVisiblityBufferROView(),
		m_RenderContext,
		commandList.get());

	commandList->EndRenderPass();
}

void st::gfx::GBuffersRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer0Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer1Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer2Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer3Texture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "GBuffersRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("GBuffers_vs", rhi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("GBuffers_ps", rhi::ShaderType::Pixel);
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
			.cullMode = rhi::CullMode::Back
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = true,
			.depthWriteEnable = false,
			.depthFunc = rhi::ComparisonFunc::Equal,
			.stencilEnable = false
		};

		m_PSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_VS.get_weak(),
			.PS = m_PS.get_weak(),
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_RenderContext = CreateRenderContext(m_PSODesc, m_FB->GetFramebufferInfo(), device, "GBuffersRenderStage");
	}
}

void st::gfx::GBuffersRenderStage::OnDetached()
{
	st::rhi::Device* device = GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	m_RenderContext = {};
}

void st::gfx::GBuffersRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer0Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer1Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer2Texture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_GBuffer3Texture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "GBuffersRenderStage");
	}

	// Re-create PSO
	m_RenderContext = CreateRenderContext(m_PSODesc, m_FB->GetFramebufferInfo(), device, "GBuffersRenderStage");
}