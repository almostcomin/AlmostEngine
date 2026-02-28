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

void st::gfx::DepthPrepassRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
	{
		//LOG_WARNING("No scene set. Nothing to render");
		return;
	}

	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() }, // Inverse-z
		{},
		rhi::RenderPassFlags::None);

	RenderSetInstanced(
		m_RenderView->GetCameraVisibleSet(),
		m_RenderView->GetSceneBufferUniformView(),
		m_RenderView->GetCameraVisiblityBufferROView(),
		m_RenderContext,
		commandList.get());

	commandList->EndRenderPass();
}

void st::gfx::DepthPrepassRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create depth stencil
	m_RenderView->CreateDepthTarget("SceneDepth", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::D24S8);

	// Request access
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneDepth", rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	{
		st::rhi::TextureHandle depthStencil = m_RenderView->GetTexture("SceneDepth");
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
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
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	m_RenderContext = {};
}

void st::gfx::DepthPrepassRenderStage::OnBackbufferResize()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		st::rhi::TextureHandle depthStencil = m_RenderView->GetTexture("SceneDepth");
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Re-create PSO
	m_RenderContext = CreateRenderContext(m_PSODesc, m_FB->GetFramebufferInfo(), device, "DepthPrepassRenderStagePSO");
}