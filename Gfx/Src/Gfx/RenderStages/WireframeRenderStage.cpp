#include "Gfx/GfxPCH.h"
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
#include "Gfx/RenderGraphBuilder.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void alm::gfx::WireframeRenderStage::Setup(RenderGraphBuilder& builder)
{
	// Create textures
	{
		// Note that we create the ToneMapped resource here (as it does ToneMappingRenderStage).
		// That should not be a probleam as far as the properties are the same in both places
		m_ToneMappedTexture = builder.CreateTexture("ToneMapped", RenderGraph::TextureResourceType::RenderTarget,
			RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);
		m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

		m_PayloadBuffer = builder.GetBufferHandle("PayloadBuffer");
		m_IndirectArgsBuffer = builder.GetBufferHandle("IndirectArgsBuffer");
	}

	// Request texture access access
	{
		builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
			rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
		builder.AddTextureDependency(m_ToneMappedTexture, RenderGraph::AccessMode::Write,
			rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);

		builder.AddBufferDependency(m_PayloadBuffer, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddBufferDependency(m_IndirectArgsBuffer, RenderGraph::AccessMode::Read,
			rhi::ResourceState::INDIRECT_ARGUMENT, rhi::ResourceState::INDIRECT_ARGUMENT);
	}
}

void alm::gfx::WireframeRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		return;
	}
	auto* deviceManager = GetDeviceManager();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store },
		{},
		rhi::RenderPassFlags::None);

	interop::WireframeStageConstats shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.instancesDI = GetRenderView()->GetCameraVisiblityBufferROView();
	shaderConstants.payloadDI = m_RenderGraph->GetBufferReadOnlyView(m_PayloadBuffer);

	commandList->PushGraphicsConstants(0, shaderConstants);

	if (deviceManager->GPUDrivenEnabled())
	{
		MaterialPassRenderer::IndirectDrawParams params{
			.ArgsBuffer = m_RenderGraph->GetBuffer(m_IndirectArgsBuffer).get(),
			.Buckets = deviceManager->GetGpuSceneBuffers()->GetBucketInfo(scene->GetGpuSceneBuffersHandle()) };

		m_MaterialPassRenderer.DrawIndirect(params, commandList.get());
	}
	else
	{
		m_MaterialPassRenderer.DrawRenderSetInstanced(GetRenderView()->GetCameraVisibleSet(), commandList.get());
	}

	commandList->EndRenderPass();
}

void alm::gfx::WireframeRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		alm::rhi::TextureHandle renderTarget = m_RenderGraph->GetTexture(m_ToneMappedTexture);
		alm::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(renderTarget)
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "WireframeRenderStage");
	}

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		if (deviceManager->GPUDrivenEnabled())
		{
			m_VS = shaderFactory->LoadShader("WireframeStage_GC_vs", rhi::ShaderType::Vertex);
		}
		else
		{
			m_VS = shaderFactory->LoadShader("WireframeStage_vs", rhi::ShaderType::Vertex);
		}

		m_VS_Terrain = shaderFactory->LoadShader("Terrain_POSO_vs", rhi::ShaderType::Vertex);
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
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_MaterialPassRenderer.Init(m_PSODesc, m_FB->GetFramebufferInfo(), "WireframeRenderStage", device);
		m_MaterialPassRenderer.AddDomain(MaterialDomain::Opaque, m_VS.get_weak(), m_PS.get_weak());
		m_MaterialPassRenderer.AddDomain(MaterialDomain::Terrain, m_VS_Terrain.get_weak(), m_PS.get_weak());
	}
}

void alm::gfx::WireframeRenderStage::OnDetached()
{
	alm::rhi::Device* device = GetDeviceManager()->GetDevice();

	m_MaterialPassRenderer = {};

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_PS));
	device->ReleaseQueued(std::move(m_VS_Terrain));
	device->ReleaseQueued(std::move(m_VS));
}

void alm::gfx::WireframeRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		alm::rhi::TextureHandle renderTarget = m_RenderGraph->GetTexture(m_ToneMappedTexture);
		alm::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(renderTarget)
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "WireframeRenderStage");
	}

	// Re-create PSO
	m_MaterialPassRenderer.OnFramebufferChanged(m_FB->GetFramebufferInfo());
}