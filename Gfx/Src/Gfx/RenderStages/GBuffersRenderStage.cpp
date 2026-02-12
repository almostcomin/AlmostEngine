#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"

void st::gfx::GBuffersRenderStage::Render()
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
		{ 
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() }
		},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::SingleInstanceDrawData shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferUniformView();

	const auto& visibleSet = m_RenderView->GetCameraVisibleSet();
	for (const st::gfx::MeshInstance* meshInstance : visibleSet)
	{
		shaderConstants.instanceIdx = scene->GetInstanceIndex(meshInstance);

		commandList->PushGraphicsConstants(shaderConstants);
		commandList->Draw(meshInstance->GetMesh()->GetIndexCount());
	}

	commandList->EndRenderPass();
}

void st::gfx::GBuffersRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create render targets
	{
		// GBuffer0
		//	RGB = BaseColor (albedo)
		//	A = MaterialID (0–255)
		m_RenderView->CreateColorTarget("GBuffer0", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		// GBuffer1
		//	RG = Normal.xy(octahedron encoded)
		//	B = Roughness
		//	A = unused / AO
		m_RenderView->CreateColorTarget("GBuffer1", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		// GBuffer2
		//	R = Metallic
		//	G = Ambient Occlusion
		//	B = Specular (optional, default 0.5)
		//	A = unused / flags
		m_RenderView->CreateColorTarget("GBuffer2", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA8_UNORM);

		// GBuffer3
		//	RGB = Emissive color
		//	A = unused
		m_RenderView->CreateColorTarget("GBuffer3", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);
	}

	// Request RT access
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "GBuffer0", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "GBuffer1", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "GBuffer2", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "GBuffer3", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth", rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer0"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer1"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer2"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer3"))
			.SetDepthAttachment(m_RenderView->GetTexture("SceneDepth"));
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

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "GBuffersRenderStage");
	}
}

void st::gfx::GBuffersRenderStage::OnDetached()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PSO));
}

void st::gfx::GBuffersRenderStage::OnBackbufferResize()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer0"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer1"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer2"))
			.AddColorAttachment(m_RenderView->GetTexture("GBuffer3"))
			.SetDepthAttachment(m_RenderView->GetTexture("SceneDepth"));
		m_FB = device->CreateFramebuffer(fbDesc, "GBuffersRenderStage");
	}

	// Re-create PSO
	m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "GBuffersRenderStage");
}