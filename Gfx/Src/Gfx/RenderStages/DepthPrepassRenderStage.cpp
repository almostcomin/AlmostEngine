#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
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

void st::gfx::DepthPrepassRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneDepthTexture = builder.CreateDepthTarget("SceneDepth", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::D24S8);

	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void st::gfx::DepthPrepassRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		//LOG_WARNING("No scene set. Nothing to render");
		return;
	}

	commandList->BeginRenderPass(
		m_FB.get(),
		{},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() }, // Inverse-z
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

void st::gfx::DepthPrepassRenderStage::OnAttached()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Create Framebuffer
	{
		st::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = GetDeviceManager()->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("DepthPrepass_vs", rhi::ShaderType::Vertex);
	}

	// Create PSO
	{
		rhi::RasterizerState rasterState =
		{
			.cullMode = rhi::CullMode::Back
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthFunc = rhi::ComparisonFunc::Greater,
			.stencilEnable = false
		};

		m_PSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_VS.get_weak(),
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_RenderContext = CreateRenderContext(m_PSODesc, m_FB->GetFramebufferInfo(), device, "DepthPrepassRenderStagePSO");
	}
}

void st::gfx::DepthPrepassRenderStage::OnDetached()
{
	GetDeviceManager()->GetDevice()->ReleaseQueued(std::move(m_FB));
	m_RenderContext = {};
}

void st::gfx::DepthPrepassRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		st::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Re-create PSO
	m_RenderContext = CreateRenderContext(m_PSODesc, m_FB->GetFramebufferInfo(), device, "DepthPrepassRenderStagePSO");
}