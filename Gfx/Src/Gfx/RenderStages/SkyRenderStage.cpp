#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderView.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void alm::gfx::SkyRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	m_FB = builder.RequestFramebuffer({ m_SceneColorTexture }, m_SceneDepthTexture);

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void alm::gfx::SkyRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
		return;

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{}, rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::SkyConstants shaderConstants;
	shaderConstants.aspect = (float)m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo().width /
		m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo().height;

	commandList->PushGraphicsConstants(0, shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void alm::gfx::SkyRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* shaderFactory = deviceManager->GetShaderFactory();

	m_PS = shaderFactory->LoadShader("Sky_ps", rhi::ShaderType::Pixel);

	{
		rhi::DepthStencilState depthStencilState{
			.depthTestEnable = true,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual };

		rhi::GraphicsPipelineStateDesc psoDesc{
			.VS = commonResources->GetBlitVS(),
			.PS = m_PS.get_weak(),
			.depthStencilState = depthStencilState };

		m_PSO = device->CreateGraphicsPipelineState(psoDesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "SkyRenderStage");
	}
}

void alm::gfx::SkyRenderStage::OnDetached()
{
	m_PSO.reset();
	m_PS.reset();
}