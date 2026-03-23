#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/RenderGraphBuilder.h"

#define DEBUG_STAGE

alm::gfx::ShadowmapRenderStage::ShadowmapRenderStage(size_t resolution, size_t numCascades, rhi::Format pixelFormat) :
	m_TextureWidth{ resolution },
	m_TextureHeight{ resolution },
	m_NumCascades{ numCascades },
	m_PixelFormat{ pixelFormat },
	m_DepthBias{ -10000 },
	m_SlopeScaledDepthBias{ -5.f }
{}

void alm::gfx::ShadowmapRenderStage::SetSize(const int2& textureSize)
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	m_TextureWidth = textureSize.x;
	m_TextureHeight = textureSize.y;

	if (IsEnabled() && IsAttached())
	{
		// Create render target
		m_RenderGraph->RecreateTexture(m_ShadowMapTexture, m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, m_PixelFormat);
#ifdef DEBUG_STAGE
		m_RenderGraph->RecreateTexture(m_ShadowMapColorTexture, m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, rhi::Format::RGBA8_UNORM);
#endif

		// Recreate Framebuffer
		{
			auto fbDesc = rhi::FramebufferDesc()
#ifdef DEBUG_STAGE
				.AddColorAttachment(m_RenderGraph->GetTexture(m_ShadowMapColorTexture))
#endif
				.SetDepthAttachment(m_RenderGraph->GetTexture(m_ShadowMapTexture));
			m_FB = device->CreateFramebuffer(fbDesc, "ShadowmapRenderStage");
		}

		// Recreate PSO (psodesc keeps the same)
		m_RenderContext.OnFramebufferChanged(m_FB->GetFramebufferInfo());
	}
}

void alm::gfx::ShadowmapRenderStage::SetDepthBias(int v)
{
	m_DepthBias = v;
	RecreatePSO();
}

void alm::gfx::ShadowmapRenderStage::SetSlopeScaledDepthBias(float v)
{
	m_SlopeScaledDepthBias = v;
	RecreatePSO();
}

void alm::gfx::ShadowmapRenderStage::InitResources()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
#ifdef DEBUG_STAGE
			.AddColorAttachment(m_RenderGraph->GetTexture(m_ShadowMapColorTexture))
#endif
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_ShadowMapTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "ShadowmapRenderStage");
	}

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
#ifdef DEBUG_STAGE
		m_VS_Opaque = shaderFactory->LoadShader("CascadeShadowmap_OP_CO_vs", rhi::ShaderType::Vertex);
		m_VS_AlphaTest = shaderFactory->LoadShader("CascadeShadowmap_AT_CO_vs", rhi::ShaderType::Vertex);
		m_PS_Opaque = shaderFactory->LoadShader("CascadeShadowmap_OP_CO_ps", rhi::ShaderType::Pixel);
		m_PS_AlphaTest = shaderFactory->LoadShader("CascadeShadowmap_AT_CO_ps", rhi::ShaderType::Pixel);
#else
		m_VS_Opaque = shaderFactory->LoadShader("CascadeShadowmap_OP_vs", rhi::ShaderType::Vertex);
		m_VS_AlphaTest = shaderFactory->LoadShader("CascadeShadowmap_AT_vs", rhi::ShaderType::Vertex);
		//m_PS_Opaque = shaderFactory->LoadShader("CascadeShadowmap_OP_ps", rhi::ShaderType::Pixel);
		m_PS_AlphaTest = shaderFactory->LoadShader("CascadeShadowmap_AT_ps", rhi::ShaderType::Pixel);
#endif
	}

	// Create PSO
	RecreatePSO();
}

void alm::gfx::ShadowmapRenderStage::ReleaseResources()
{
	alm::rhi::Device* device = GetDeviceManager()->GetDevice();

	m_RenderContext = {};

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PS_AlphaTest));
	device->ReleaseQueued(std::move(m_VS_AlphaTest));
	device->ReleaseQueued(std::move(m_PS_Opaque));
	device->ReleaseQueued(std::move(m_VS_Opaque));
}

void alm::gfx::ShadowmapRenderStage::RecreatePSO()
{
	alm::rhi::Device* device = GetDeviceManager()->GetDevice();

	rhi::BlendState blendState;
	blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
	{
		.blendEnable = false,
	};

	rhi::RasterizerState rasterState =
	{
		.cullMode = rhi::CullMode::Back,
		.depthBias = m_DepthBias,
		.slopeScaledDepthBias = m_SlopeScaledDepthBias
	};

	rhi::DepthStencilState depthStencilState =
	{
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthFunc = rhi::ComparisonFunc::Greater
	};

	m_PSODesc = rhi::GraphicsPipelineStateDesc
	{
		.blendState = blendState,
		.depthStencilState = depthStencilState,
		.rasterState = rasterState
	};

	m_RenderContext.Reset();
	m_RenderContext.Init(m_PSODesc, m_FB->GetFramebufferInfo(), "ShadowmapRenderStage", device);
	m_RenderContext.AddDomain(MaterialDomain::Opaque, m_VS_Opaque.get_weak(), m_PS_Opaque.get_weak());
	m_RenderContext.AddDomain(MaterialDomain::AlphaTested, m_VS_AlphaTest.get_weak(), m_PS_AlphaTest.get_weak());
}

void alm::gfx::ShadowmapRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_ShadowMapTexture = builder.CreateDepthTarget("Shadowmap", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, m_PixelFormat);
#ifdef DEBUG_STAGE
	m_ShadowMapColorTexture = builder.CreateColorTarget("ShadowmapColor", m_TextureWidth, m_TextureHeight, 1/*m_NumCascades*/, rhi::Format::RGBA8_UNORM);
#endif

	builder.AddTextureDependency(m_ShadowMapTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
#ifdef DEBUG_STAGE
	builder.AddTextureDependency(m_ShadowMapColorTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
#endif
}

void alm::gfx::ShadowmapRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		//LOG_WARNING("No scene set. Nothing to render");
		return;
	}

	rhi::Device* device = GetDeviceManager()->GetDevice();

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

	interop::ShadowmapStageConstats shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.instancesDI = GetRenderView()->GetShadowMapVisibilityBufferROView();

	commandList->PushGraphicsConstants(0, shaderConstants);

	m_RenderContext.DrawRenderSetInstanced(
		GetRenderView()->GetShadowMapVisibleSet(),
		commandList.get());

	commandList->EndRenderPass();
}

void alm::gfx::ShadowmapRenderStage::OnAttached()
{
	if (IsEnabled())
		InitResources();
}

void alm::gfx::ShadowmapRenderStage::OnDetached()
{
	ReleaseResources();
}

void alm::gfx::ShadowmapRenderStage::OnBackbufferResize()
{
	// No need to recreate framebuffer and PSO because shadowmap resolution
	// doesn't change with backbuffer size
}

void alm::gfx::ShadowmapRenderStage::OnEnabled()
{
	if (IsAttached())
	{
		m_RenderGraph->EnableTexture(m_ShadowMapTexture);
#ifdef DEBUG_STAGE
		m_RenderGraph->EnableTexture(m_ShadowMapColorTexture);
#endif

		InitResources();
	}
}

void alm::gfx::ShadowmapRenderStage::OnDisabled()
{
	m_RenderGraph->DisableTexture(m_ShadowMapTexture);
#ifdef DEBUG_STAGE
	m_RenderGraph->DisableTexture(m_ShadowMapColorTexture);
#endif

	ReleaseResources();
}