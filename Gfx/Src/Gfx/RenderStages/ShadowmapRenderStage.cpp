#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"

#define DEBUG_STAGE

st::gfx::ShadowmapRenderStage::ShadowmapRenderStage(size_t resolution, size_t numCascades, rhi::Format pixelFormat) :
	m_TextureWidth{ resolution },
	m_TextureHeight{ resolution },
	m_NumCascades{ numCascades },
	m_PixelFormat{ pixelFormat }
{}

void st::gfx::ShadowmapRenderStage::SetSize(const int2& textureSize)
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	m_TextureWidth = textureSize.x;
	m_TextureHeight = textureSize.y;

	if (IsEnabled() && IsAttached())
	{
		// Create render target
		m_RenderView->RecreateDepthTarget("Shadowmap", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, m_PixelFormat);
#ifdef DEBUG_STAGE
		m_RenderView->RecreateColorTarget("ShadowmapColor", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, rhi::Format::RGBA8_UNORM);
#endif

		// Recreate Framebuffer
		{
			auto fbDesc = rhi::FramebufferDesc()
#ifdef DEBUG_STAGE
				.AddColorAttachment(m_RenderView->GetTexture("ShadowmapColor"))
#endif
				.SetDepthAttachment(m_RenderView->GetTexture("Shadowmap"));
			m_FB = device->CreateFramebuffer(fbDesc, "ShadowmapRenderStage");
		}

		// Recreate PSO (psodesc keeps the same)
		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "ShadowmapRenderStage");
	}
}

void st::gfx::ShadowmapRenderStage::InitResources()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	st::gfx::CommonResources* commonResources = deviceManager->GetCommonResources();
	rhi::Device* device = deviceManager->GetDevice();

	// Create render target
	m_RenderView->CreateDepthTarget("Shadowmap", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, m_PixelFormat);
#ifdef DEBUG_STAGE
	m_RenderView->CreateColorTarget("ShadowmapColor", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, rhi::Format::RGBA8_UNORM);
#endif

	// Request access
#ifdef DEBUG_STAGE
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ShadowmapColor", rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
#endif
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "Shadowmap", rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
#ifdef DEBUG_STAGE
			.AddColorAttachment(m_RenderView->GetTexture("ShadowmapColor"))
#endif
			.SetDepthAttachment(m_RenderView->GetTexture("Shadowmap"));
		m_FB = device->CreateFramebuffer(fbDesc, "ShadowmapRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("CascadeShadowmap_vs", rhi::ShaderType::Vertex);
#ifdef DEBUG_STAGE
		m_PS = shaderFactory->LoadShader("CascadeShadowmap_ps", rhi::ShaderType::Pixel);
#endif
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
			.cullMode = rhi::CullMode::Back,
			.depthBias = -1000,
			.slopeScaledDepthBias = -1.f
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthFunc = rhi::ComparisonFunc::Greater
		};

		m_PSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_VS.get_weak(),
#ifdef DEBUG_STAGE
			.PS = m_PS.get_weak(),
#endif
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_PSO = device->CreateGraphicsPipelineState(m_PSODesc, m_FB->GetFramebufferInfo(), "ShadowmapRenderStage");
	}
}

void st::gfx::ShadowmapRenderStage::ReleaseResources()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_FB);
	device->ReleaseQueued(m_PSO);
	device->ReleaseQueued(m_PS);
	device->ReleaseQueued(m_VS);

	m_RenderView->ReleaseTexture("Shadowmap");
#ifdef DEBUG_STAGE
	m_RenderView->ReleaseTexture("ShadowmapColor");
#endif
}

void st::gfx::ShadowmapRenderStage::Render()
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
#ifdef DEBUG_STAGE
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() }},
#else
		{},
#endif
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));

	interop::SingleInstanceDrawData shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferDI();

	const auto& visibleSet = m_RenderView->GetVisibleSet();
	for (const st::gfx::MeshInstance* meshInstance : visibleSet)
	{
		shaderConstants.instanceIdx = scene->GetInstanceIndex(meshInstance);

		commandList->PushConstants(shaderConstants);
		commandList->Draw(meshInstance->GetMesh()->GetIndexCount());
	}

	commandList->EndRenderPass();
}

void st::gfx::ShadowmapRenderStage::OnAttached()
{
	if (IsEnabled())
		InitResources();
}

void st::gfx::ShadowmapRenderStage::OnDetached()
{
	ReleaseResources();
}

void st::gfx::ShadowmapRenderStage::OnBackbufferResize()
{
	// No need to recreate framebuffer and PSO because shadowmap resolution
	// doesn't change with backbuffer size
}

void st::gfx::ShadowmapRenderStage::OnEnabled()
{
	if (IsAttached())
		InitResources();
}

void st::gfx::ShadowmapRenderStage::OnDisabled()
{
	ReleaseResources();
}