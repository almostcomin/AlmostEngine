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

	interop::SkyData* skyData = (interop::SkyData*)m_ShaderCB.Map();
	{
		const Scene::SunParams& sunParams = scene->GetSunParams();

		float lightAngularSize = glm::radians(glm::clamp(sunParams.AngularSizeDeg, 0.1f, 90.f));
		float lightSolidAngle = 4 * PI * square(sinf(lightAngularSize * 0.5f));
		float lightRadiance = sunParams.Irradiance / lightSolidAngle;
		if (m_Params.maxLightRadiance > 0.f)
			lightRadiance = std::min(lightRadiance, m_Params.maxLightRadiance);
		const float3 sunDir = alm::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

		skyData->directionToLight = -sunDir;
		skyData->angularSizeOfLight = lightAngularSize;
		skyData->lightColor = lightRadiance * sunParams.Color;
		skyData->glowSize = glm::radians(glm::clamp(m_Params.glowSize, 0.f, 90.f));
		skyData->skyColor = m_Params.skyColor * m_Params.brightness;
		skyData->glowIntensity = glm::clamp(m_Params.glowIntensity, 0.f, 1.f);
		skyData->horizonColor = m_Params.horizonColor * m_Params.brightness;
		skyData->horizonSize = glm::radians(glm::clamp(m_Params.horizonSize, 0.f, 90.f));
		skyData->groundColor = m_Params.groundColor * m_Params.brightness;
		skyData->glowSharpness = glm::clamp(m_Params.glowSharpness, 1.f, 10.f);
		skyData->directionUp = glm::normalize(m_Params.directionUp);
		skyData->resolution = float2(
			m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo().width,
			m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo().height);
		skyData->aspect = skyData->resolution.x / skyData->resolution.y;
	}
	m_ShaderCB.Unmap();

	interop::SkyConstants shaderConstants;
	float4x4 invView = glm::inverse(GetCamera()->GetViewMatrix());
	invView[3] = float4(0, 0, 0, 1);
	float4x4 invProj = glm::inverse(GetCamera()->GetProjectionMatrix());

	shaderConstants.matClipToTranslatedWorld = invView * invProj;
	shaderConstants.skyDataDI = m_ShaderCB.GetUniformView();

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