#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/RenderView.h"
#include "Gfx/ShaderFactory.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/RenderGraphBuilder.h"

void alm::gfx::DepthPrepassRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneDepthTexture = builder.CreateDepthTarget("SceneDepth", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::D24S8);
	m_PayloadBuffer = builder.GetBufferHandle("PayloadBuffer");
	m_IndirectArgsBuffer = builder.GetBufferHandle("IndirectArgsBuffer");

	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
	builder.AddBufferDependency(m_PayloadBuffer, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddBufferDependency(m_IndirectArgsBuffer, RenderGraph::AccessMode::Read,
		rhi::ResourceState::INDIRECT_ARGUMENT, rhi::ResourceState::INDIRECT_ARGUMENT);
}

void alm::gfx::DepthPrepassRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
		return;
	auto* deviceManager = GetDeviceManager();

	commandList->BeginRenderPass(
		m_FB.get(),
		{},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() }, // Inverse-z
		{},
		rhi::RenderPassFlags::None);

	interop::DepthPrepassStageConstants shaderConstants;
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

void alm::gfx::DepthPrepassRenderStage::OnAttached()
{
	auto* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		alm::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = GetDeviceManager()->GetShaderFactory();
		if (deviceManager->GPUDrivenEnabled())
		{
			m_VS_Opaque = shaderFactory->LoadShader("DepthPrepass_OP_GC_vs", rhi::ShaderType::Vertex);
			m_VS_AlphaTest = shaderFactory->LoadShader("DepthPrepass_AT_GC_vs", rhi::ShaderType::Vertex);
		}
		else
		{
			m_VS_Opaque = shaderFactory->LoadShader("DepthPrepass_OP_vs", rhi::ShaderType::Vertex);
			m_VS_AlphaTest = shaderFactory->LoadShader("DepthPrepass_AT_vs", rhi::ShaderType::Vertex);
		}
		m_PS_AlphaTest = shaderFactory->LoadShader("DepthPrepass_AT_ps", rhi::ShaderType::Pixel);
		m_VS_Terrain = shaderFactory->LoadShader("Terrain_POSO_vs", rhi::ShaderType::Vertex);
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

		m_MaterialPassRenderer.Init(m_PSODesc, m_FB->GetFramebufferInfo(), "DepthPrepassRenderStage", device);
		m_MaterialPassRenderer.AddDomain(MaterialDomain::Opaque, m_VS_Opaque.get_weak(), nullptr);
		m_MaterialPassRenderer.AddDomain(MaterialDomain::AlphaTested, m_VS_AlphaTest.get_weak(), m_PS_AlphaTest.get_weak());
		m_MaterialPassRenderer.AddDomain(MaterialDomain::Terrain, m_VS_Terrain.get_weak(), nullptr);
	}
}

void alm::gfx::DepthPrepassRenderStage::OnDetached()
{
	GetDeviceManager()->GetDevice()->ReleaseQueued(std::move(m_FB));
	m_MaterialPassRenderer = {};
}

void alm::gfx::DepthPrepassRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Re-create Framebuffer
	{
		alm::rhi::TextureHandle depthStencil = m_RenderGraph->GetTexture(m_SceneDepthTexture);
		auto fbDesc = rhi::FramebufferDesc()
			.SetDepthAttachment(depthStencil);
		m_FB = device->CreateFramebuffer(fbDesc, "DepthPrepassRenderStage");
	}

	// Re-create PSO
	m_MaterialPassRenderer.OnFramebufferChanged(m_FB->GetFramebufferInfo());
}