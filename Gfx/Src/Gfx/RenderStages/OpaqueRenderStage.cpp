#include "Gfx/RenderStages/OpaqueRenderStage.h"
#include "Core/Log.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

bool st::gfx::OpaqueRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
	{
		//LOG_WARNING("No scene set. Nothing to render");
		return false;
	}

	auto camera = m_RenderView->GetCamera();
	if (!camera)
	{
		LOG_WARNING("No camera set. Can't render without a camera.");
		return false;
	}

	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() },
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));

	interop::OpaqueStage shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferDI();
	shaderConstants.instanceBufferDI = scene->GetInstancesBufferDI();
	shaderConstants.meshesBufferDI = scene->GetMeshesBufferDI();

	const auto& visibleSet = m_RenderView->GetVisibleSet();
	for (const st::gfx::MeshInstance* meshInstance : visibleSet)
	{
		shaderConstants.instanceIdx = scene->GetInstanceIndex(meshInstance);

		commandList->PushConstants(shaderConstants);
		commandList->Draw(meshInstance->GetMesh()->GetIndexCount());
	}

	commandList->EndRenderPass();

	return true;
}

void st::gfx::OpaqueRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create render targets
	m_RenderView->CreateColorTarget("SceneColor", RenderView::c_BBSize, RenderView::c_BBSize, rhi::Format::SRGBA8_UNORM);
	m_RenderTarget = m_RenderView->GetTexture("SceneColor");
	m_RenderView->CreateDepthTarget("SceneDepth", RenderView::c_BBSize, RenderView::c_BBSize, rhi::Format::D24S8);
	m_DepthStencil = m_RenderView->GetTexture("SceneDepth");

	// Request RT access
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneDepth", rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderTarget.get())
			.SetDepthAttachment(m_DepthStencil.get());
		m_FB = device->CreateFramebuffer(fbDesc, "OpaqueRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("OpaqueStage_vs.vso", rhi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("OpaqueStage_ps.vso", rhi::ShaderType::Pixel);
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
			.depthWriteEnable = true,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual,
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

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "OpaqueRenderStage");
	}
}

void st::gfx::OpaqueRenderStage::OnDetached()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_FB);

	m_RenderTarget = nullptr;
	m_DepthStencil = nullptr;

	device->ReleaseQueued(m_PSO);
}

void st::gfx::OpaqueRenderStage::OnBackbufferResize()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	m_RenderTarget = m_RenderView->GetTexture("SceneColor");
	m_DepthStencil = m_RenderView->GetTexture("SceneDepth");

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderTarget.get())
			.SetDepthAttachment(m_DepthStencil.get());
		m_FB = device->CreateFramebuffer(fbDesc, "OpaqueRenderStage");
	}

	// Re-create PSO
	m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "OpaqueRenderStage");
}