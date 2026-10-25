#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CubemapSkyRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderView.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Gfx/LoadedTexture.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void alm::gfx::CubemapSkyRenderStage::SetCubemapTexture(std::shared_ptr<alm::gfx::LoadedTexture> cubemap)
{
	m_CubemapTexture = {};
	if (cubemap && cubemap->texture)
	{
		if (cubemap->texture->GetDesc().dimension == rhi::TextureDimension::TextureCube)
		{
			m_CubemapTexture = cubemap;
		}
		else
		{
			LOG_WARNING("Invalid texture '{}': Is not a cubemap", cubemap->texture->GetDebugName());
		}
	}
}

std::shared_ptr<alm::gfx::LoadedTexture> alm::gfx::CubemapSkyRenderStage::GetCubemapTexture() const
{
	return m_CubemapTexture;
}

void alm::gfx::CubemapSkyRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	m_FB = builder.RequestFramebuffer({ m_SceneColorTexture }, m_SceneDepthTexture, "CubemapSkyFB");

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void alm::gfx::CubemapSkyRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto scene = GetScene();
	if (!scene)
		return;

	if (!m_CubemapTexture || !m_CubemapTexture->texture)
	{
		//LOG_WARNING("No texture set in CubemapSkyRenderStage");
		return;
	}

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{}, rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::CubemapSkyConstants shaderConstants;
	shaderConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
	shaderConstants.cubemapTextureDI = m_CubemapTexture->texture->GetSampledView();
	shaderConstants.exposureFactorTextureDI = GetRenderView()->GetPrevExposureSampledView();

	commandList->PushGraphicsConstants(0, shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void alm::gfx::CubemapSkyRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* shaderFactory = deviceManager->GetShaderFactory();

	m_PS = shaderFactory->LoadShader("CubemapSky_ps", rhi::ShaderType::Pixel);

	{
		rhi::DepthStencilState depthStencilState{
			.depthTestEnable = true,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual };

		rhi::GraphicsPipelineStateDesc psoDesc{
			.VS = commonResources->GetBlitVS(),
			.PS = m_PS.get_weak(),
			.depthStencilState = depthStencilState };

		m_PSO = device->CreateGraphicsPipelineState(psoDesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "CubemapSkyRenderStage");
	}
}

void alm::gfx::CubemapSkyRenderStage::OnDetached()
{
	m_PSO.reset();
	m_PS.reset();
}