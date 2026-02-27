#include "Gfx/RenderStages/WireframeRenderStage.h"
#include "Core/Log.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"
#include "Gfx/RenderHelpers.h"

void st::gfx::WireframeRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
	{
		return;
	}

	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	RenderSetInstanced(
		m_RenderView->GetCameraVisibleSet(),
		m_RenderView->GetSceneBufferUniformView(),
		m_RenderView->GetCameraVisiblityBufferROView(),
		commandList.get());

	commandList->EndRenderPass();
}

void st::gfx::WireframeRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create textures
	{
		// Note that we create the ToneMapped resource here (as it does ToneMappingRenderStage).
		// That should not be a probleam as far as the properties are the same in both places
		m_RenderView->CreateTexture("ToneMapped", RenderView::TextureResourceType::RenderTarget,
			RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);
	}

	// Request texture access access
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth",
			rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ToneMapped",
			rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	}

	// Create Framebuffer
	{
		st::rhi::TextureHandle renderTarget = m_RenderView->GetTexture("ToneMapped");
		st::rhi::TextureHandle depthStencil = m_RenderView->GetTexture("SceneDepth");
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(renderTarget)
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "WireframeRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("WireframeStage_vs", rhi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("WireframeStage_ps", rhi::ShaderType::Pixel);
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
			.fillMode = rhi::FillMode::Wireframe,
			.cullMode = rhi::CullMode::Back,
			.depthBias = 100,
			.depthBiasClamp = 0.001,
			.slopeScaledDepthBias = 1.0f
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = true,
			.depthWriteEnable = false,
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

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "WireframeRenderStage");
	}
}

void st::gfx::WireframeRenderStage::OnDetached()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PSO));
	device->ReleaseQueued(std::move(m_PS));
	device->ReleaseQueued(std::move(m_VS));
}

void st::gfx::WireframeRenderStage::OnBackbufferResize()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		st::rhi::TextureHandle renderTarget = m_RenderView->GetTexture("ToneMapped");
		st::rhi::TextureHandle depthStencil = m_RenderView->GetTexture("SceneDepth");
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(renderTarget)
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "WireframeRenderStage");
	}

	// Re-create PSO
	m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "WireframeRenderStage");
}