#include "Gfx/RenderStages/WBOITAccumRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void st::gfx::WBOITAccumRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_AccumWOITTexture = builder.CreateColorTarget(
		"AccumWOIT", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);
	m_RevealageWOITTexture = builder.CreateColorTarget(
		"RevealageWOIT", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::R8_UNORM);

	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");
	m_ShadowmapTexture = builder.GetTextureHandle("Shadowmap");
	m_AmbientOcclusionTexture = builder.GetTextureHandle("AmbientOcclusion");

	builder.AddTextureDependency(m_AccumWOITTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_RevealageWOITTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
	builder.AddTextureDependency(m_ShadowmapTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_AmbientOcclusionTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void st::gfx::WBOITAccumRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
	{
		return;
	}

	float2 shadowMapResolution = { 0.f, 0.f };
	auto shadowmapTex = m_RenderGraph->GetTexture(m_ShadowmapTexture);
	if (shadowmapTex)
	{
		shadowMapResolution = { (float)shadowmapTex->GetDesc().width, (float)shadowmapTex->GetDesc().height };
	}

	commandList->BeginRenderPass(
		m_FB.get(),
		{
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() },
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::Color(float4{1.f}) },
		},
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::DepthZero() },
		{},
		rhi::RenderPassFlags::None);

	interop::WBOITAccumStageConstants shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.shadowMapDI = m_RenderGraph->GetTextureSampledView(m_ShadowmapTexture);
	shaderConstants.oneOverShadowmapResolution = 1.f / shadowMapResolution;
	shaderConstants.SSAO_DI = m_RenderGraph->GetTextureSampledView(m_AmbientOcclusionTexture);
	shaderConstants.instancesDI = GetRenderView()->GetCameraVisiblityBufferROView();

	commandList->PushGraphicsConstants(0, shaderConstants);

	m_RenderContext.DrawRenderSetInstanced(
		GetRenderView()->GetCameraVisibleSet(),
		commandList.get());

	commandList->EndRenderPass();
}

void st::gfx::WBOITAccumRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_AccumWOITTexture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_RevealageWOITTexture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "WBOITAccumRenderStage");
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("WBOITAccumulation_vs", rhi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("WBOITAccumulation_ps", rhi::ShaderType::Pixel);
	}

	// PSOs
	{
		st::rhi::GraphicsPipelineStateDesc psoDesc;

		psoDesc.blendState = rhi::BlendState{
			.independentBlendEnable = true
		};

		psoDesc.blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState{
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::One,
			.destBlend = rhi::BlendFactor::One,
			.blendOp = rhi::BlendOp::Add,
			.srcBlendAlpha = rhi::BlendFactor::One,
			.destBlendAlpha = rhi::BlendFactor::One,
			.blendOpAlpha = rhi::BlendOp::Add
		};
		psoDesc.blendState.renderTarget[1] = rhi::BlendState::RenderTargetBlendState{
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::Zero,
			.destBlend = rhi::BlendFactor::OneMinusSrcColor,
			.blendOp = rhi::BlendOp::Add
		};

		psoDesc.depthStencilState = rhi::DepthStencilState
		{
			.depthTestEnable = true,
			.depthWriteEnable = false,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual
		};

		m_RenderContext.Init(psoDesc, m_FB->GetFramebufferInfo(), "WBOITAccumRenderStage", device);
		m_RenderContext.AddDomain(MaterialDomain::AlphaBlended, m_VS.get_weak(), m_PS.get_weak());
	}
}

void st::gfx::WBOITAccumRenderStage::OnDetached()
{
	st::rhi::Device* device = GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_FB));
	device->ReleaseQueued(std::move(m_VS));
	device->ReleaseQueued(std::move(m_PS));
	m_RenderContext = {};
}

void st::gfx::WBOITAccumRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Recreate Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_AccumWOITTexture))
			.AddColorAttachment(m_RenderGraph->GetTexture(m_RevealageWOITTexture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = device->CreateFramebuffer(fbDesc, "WBOITAccumRenderStage");
	}

	m_RenderContext.OnFramebufferChanged(m_FB->GetFramebufferInfo());
}