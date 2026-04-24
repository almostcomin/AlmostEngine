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

	const Scene::SunParams& sunParams = scene->GetSunParams();

	interop::SkyData* skyData = (interop::SkyData*)m_ShaderCB.Map();
	{
		float lightAngularSize = glm::radians(glm::clamp(sunParams.AngularSizeDeg, 0.1f, 90.f));
		float lightSolidAngle = 4 * PI * square(sinf(lightAngularSize * 0.5f));
		float lightRadiance = sunParams.Irradiance / lightSolidAngle;
		const float3 sunDir = alm::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

		skyData->ToSunDirection = -sunDir;
		skyData->SunColor = sunParams.Color;
		skyData->SunIrradiance = sunParams.Irradiance;
		skyData->ZenitColor = m_Params.ZenitColor;
		skyData->ZenitFalloff = m_Params.ZenitFalloff;
		skyData->HorizonColor = m_Params.HorizonColor;
		skyData->HorizonFalloff = m_Params.HorizonFalloff;
		skyData->GroundColor = m_Params.GroundColor;
		skyData->GroundFalloff = m_Params.GroundFalloff;
	}
	m_ShaderCB.Unmap();

	interop::SkyConstants shaderConstants;
	shaderConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
	shaderConstants.SkyDataDI = m_ShaderCB.GetUniformView();

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

	m_ShaderCB.InitUniformBuffer(sizeof(interop::SkyData), deviceManager, "SkyData");
}

void alm::gfx::SkyRenderStage::OnDetached()
{
	m_ShaderCB.Release();
	m_PSO.reset();
	m_PS.reset();
}