#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/TransmissiveRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

std::optional<alm::gfx::MaterialPassRenderer::RenderStats>
alm::gfx::TransmissiveRenderStage::GetRenderedStats(const interop::CullingStats* stats) const
{
	if (!IsEnabled())
		return {};   // disabled: valid stats, all zeros

	if (GetDeviceManager()->GPUDrivenEnabled())
	{
		assert(stats);
		return m_MaterialPassRenderer.ComputeRenderedStats(*stats, false);
	}
	else
	{
		return m_LastRenderedStats;
	}
}

void alm::gfx::TransmissiveRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");
	m_ShadowmapTexture = builder.GetTextureHandle("Shadowmap");
	m_AmbientOcclusionTexture = builder.GetTextureHandle("AmbientOcclusion");

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
	builder.AddTextureDependency(m_ShadowmapTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_AmbientOcclusionTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	if (GetDeviceManager()->GPUDrivenEnabled())
	{
		m_PayloadBuffer = builder.GetBufferHandle("PayloadBuffer");
		m_IndirectDrawArgsBuffer = builder.GetBufferHandle("IndirectDrawArgsBuffer");
		m_IndirectDrawCountsBuffer = builder.GetBufferHandle("IndirectDrawCountsBuffer");

		builder.AddBufferDependency(m_PayloadBuffer, RenderGraph::AccessMode::Read,
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		builder.AddBufferDependency(m_IndirectDrawArgsBuffer, RenderGraph::AccessMode::Read,
			rhi::ResourceState::INDIRECT_ARGUMENT, rhi::ResourceState::INDIRECT_ARGUMENT);
		builder.AddBufferDependency(m_IndirectDrawCountsBuffer, RenderGraph::AccessMode::Read,
			rhi::ResourceState::INDIRECT_ARGUMENT, rhi::ResourceState::INDIRECT_ARGUMENT);
	}

	m_FB = builder.RequestFramebuffer({ m_SceneColorTexture }, m_SceneDepthTexture, "TransmissiveRenderStage");
}

void alm::gfx::TransmissiveRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		return;
	}
	auto* deviceManager = GetDeviceManager();

	float2 shadowMapResolution = { 0.f, 0.f };
	auto shadowmapTex = m_RenderGraph->GetTexture(m_ShadowmapTexture);
	if (shadowmapTex)
	{
		shadowMapResolution = { (float)shadowmapTex->GetDesc().width, (float)shadowmapTex->GetDesc().height };
	}

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{}, rhi::RenderPassFlags::None);

	interop::TransmissiveStageConstants shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.shadowMapDI = GetRenderView()->IsShadowmapValid() ? 
		m_RenderGraph->GetTextureSampledView(m_ShadowmapTexture) : interop::TextureSampledViewIndex{};
	shaderConstants.oneOverShadowmapResolution = shadowmapTex ? 1.f / shadowMapResolution : float2{ 0.f, 0.f };
	shaderConstants.SSAO_DI = m_RenderGraph->GetTextureSampledView(m_AmbientOcclusionTexture);
	shaderConstants.instancesDI = GetRenderView()->GetCameraVisiblityBufferROView();
	shaderConstants.payloadDI = m_RenderGraph->GetBufferReadOnlyView(m_PayloadBuffer);

	commandList->PushGraphicsConstants(0, shaderConstants);

	if (deviceManager->GPUDrivenEnabled())
	{
		MaterialPassRenderer::IndirectDrawParams params{
			.ArgsBuffer = m_RenderGraph->GetBuffer(m_IndirectDrawArgsBuffer).get(),
			.CountsBuffer = m_RenderGraph->GetBuffer(m_IndirectDrawCountsBuffer).get(),
			.Buckets = deviceManager->GetGpuSceneBuffers()->GetBucketInfo(scene->GetGpuSceneBuffersHandle()),
			.Transients = deviceManager->GetGpuSceneBuffers()->GetTransientRecords(scene->GetGpuSceneBuffersHandle()) };

		m_MaterialPassRenderer.DrawIndirect(params, commandList.get());
	}
	else
	{
		m_LastRenderedStats = m_MaterialPassRenderer.DrawRenderSetInstanced(
			GetRenderView()->GetCameraVisibleSet(),
			commandList.get());
	}

	commandList->EndRenderPass();
}

void alm::gfx::TransmissiveRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		if (deviceManager->GPUDrivenEnabled())
		{
			m_VS = shaderFactory->LoadShader("Transmissive_vs", rhi::ShaderType::Vertex, { "GPU_CULL" });
		}
		else
		{
			m_VS = shaderFactory->LoadShader("Transmissive_vs", rhi::ShaderType::Vertex);
		}
		m_PS = shaderFactory->LoadShader("Transmissive_ps", rhi::ShaderType::Pixel);
		m_PS_AT = shaderFactory->LoadShader("Transmissive_ps", rhi::ShaderType::Pixel, { "ALPHA_TEST" });
	}

	// PSOs
	{
		alm::rhi::GraphicsPipelineStateDesc psoDesc;

		psoDesc.blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState{
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::One,
			.destBlend = rhi::BlendFactor::Src1Color,
			.blendOp = rhi::BlendOp::Add,
			.srcBlendAlpha = rhi::BlendFactor::Zero,
			.destBlendAlpha = rhi::BlendFactor::One,
			.blendOpAlpha = rhi::BlendOp::Add
		};

		psoDesc.depthStencilState = rhi::DepthStencilState
		{
			.depthTestEnable = true,
			.depthWriteEnable = false,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual
		};

		m_MaterialPassRenderer.Init(psoDesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(),
			"TransmissiveRenderStage", device);
		m_MaterialPassRenderer.AddDomain(MaterialDomain::OpaqueTransmission, m_VS.get_weak(), m_PS.get_weak());
		m_MaterialPassRenderer.AddDomain(MaterialDomain::AlphaTestedTransmission, m_VS.get_weak(), m_PS_AT.get_weak());
	}
}

void alm::gfx::TransmissiveRenderStage::OnDetached()
{
	alm::rhi::Device* device = GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_VS));
	device->ReleaseQueued(std::move(m_PS));
	device->ReleaseQueued(std::move(m_PS_AT));
	m_MaterialPassRenderer = {};
}

void alm::gfx::TransmissiveRenderStage::OnBackbufferResize()
{
	m_MaterialPassRenderer.OnFramebufferChanged(m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo());
}