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

	DrawRenderSetInstanced(
		GetRenderView()->GetCameraVisibleSet(),
		GetRenderView()->GetCameraVisiblityBufferROView(),
		GetRenderView()->GetSceneBufferUniformView(),
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
		m_VS_Opaque = shaderFactory->LoadShader("DepthPrepass_OP_vs", rhi::ShaderType::Vertex);
		m_VS_AlphaTest = shaderFactory->LoadShader("DepthPrepass_AT_vs", rhi::ShaderType::Vertex);
		m_PS_AlphaTest = shaderFactory->LoadShader("DepthPrepass_AT_ps", rhi::ShaderType::Pixel);
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
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_RenderContext.Init(m_PSODesc, m_FB->GetFramebufferInfo(), "DepthPrepassRenderStage", device);
		m_RenderContext.AddDomain(MaterialDomain::Opaque, m_VS_Opaque.get_weak(), nullptr);
		m_RenderContext.AddDomain(MaterialDomain::AlphaTested, m_VS_AlphaTest.get_weak(), m_PS_AlphaTest.get_weak());
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
	m_RenderContext.OnFramebufferChanged(m_FB->GetFramebufferInfo());
}