#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderView.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

alm::gfx::CloudsRenderStage::CloudsRenderStage() : m_CloudsTextureIdx{ -1 }, m_DebugChannel{ DebugChannel::Disabled }
{}

void alm::gfx::CloudsRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_LinearDepthTexture = builder.GetTextureHandle("LinearDepth");

	m_CompositeFB = builder.RequestFramebuffer({ m_SceneColorTexture });

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_LinearDepthTexture, RenderGraph::AccessMode::Read, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::CloudsRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!GetScene())
		return;
	if (!GetCamera())
		return;

	const gfx::AtmosphereConfig& atmos = GetScene()->GetAtmosphereConfig();
	rhi::TextureHandle cloudsShape = atmos.GetCloudsShapeTexture();
	rhi::TextureHandle cloudsDetail = atmos.GetCloudsDetailTexture();
	if (!cloudsShape || !cloudsDetail)
	{
		LOG_WARNING("CloudsRenderStage: No clouds texture defined");
		return;
	}

	// Initialize buffers
	bool clearCloudsTextures = false;
	if (m_CloudsTextureIdx < 0)
	{
		clearCloudsTextures = true;
		m_CloudsTextureIdx = 0;
	}
	else
	{
		m_CloudsTextureIdx = ++m_CloudsTextureIdx % 2;
	}
	int cloudsOtherIdx = (m_CloudsTextureIdx + 1) % 2;

	// Clear SRV texture if requested
	if (clearCloudsTextures)
	{
		if (m_CloudsTextureState[cloudsOtherIdx] != rhi::ResourceState::RENDERTARGET)
		{
			commandList->PushBarrier(rhi::Barrier::Texture(
				m_CloudsTexture[cloudsOtherIdx].get(), m_CloudsTextureState[cloudsOtherIdx], rhi::ResourceState::RENDERTARGET));
			m_CloudsTextureState[cloudsOtherIdx] = rhi::ResourceState::RENDERTARGET;
		}

		commandList->ClearRenderTarget(m_CloudsFB[cloudsOtherIdx]->GetColorTargetView(0), float4{ 0.f, 0.f, 0.f, 1.f });
	}

	// Do initial transitions
	std::vector<rhi::Barrier> barriers;
	barriers.reserve(2);

	if (m_CloudsTextureState[m_CloudsTextureIdx] != rhi::ResourceState::RENDERTARGET)
	{
		barriers.push_back(rhi::Barrier::Texture(
			m_CloudsTexture[m_CloudsTextureIdx].get(), m_CloudsTextureState[m_CloudsTextureIdx], rhi::ResourceState::RENDERTARGET));
		m_CloudsTextureState[m_CloudsTextureIdx] = rhi::ResourceState::RENDERTARGET;
	}
	if (m_CloudsTextureState[cloudsOtherIdx] != rhi::ResourceState::SHADER_RESOURCE)
	{
		barriers.push_back(rhi::Barrier::Texture(
			m_CloudsTexture[cloudsOtherIdx].get(), m_CloudsTextureState[cloudsOtherIdx], rhi::ResourceState::SHADER_RESOURCE));
		m_CloudsTextureState[cloudsOtherIdx] = rhi::ResourceState::SHADER_RESOURCE;
	}
	if (!barriers.empty())
	{
		commandList->PushBarriers(barriers);
	}

	// Render clouds pass
	{
		commandList->BeginRenderPass(
			m_CloudsFB[m_CloudsTextureIdx].get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack() }},
			rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
			{}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_CloudsPSO.get());

		const gfx::AtmosphereConfig::SunParams& sunParams = atmos.Sun;
		const gfx::AtmosphereConfig::CloudsParams& cloudsParams = atmos.Clouds;

		// Update clouds position offset
		m_CloudsOffset += atmos.WindVelocity * GetRenderView()->GetTimeDelta();

		const float3 toSunDirection = -glm::normalize(alm::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg)));
		const float muS = cloudsParams.ScatteringCoeff / atmos.EarthScaleFactor;
		const float muA = cloudsParams.AbsorptionCoeff / atmos.EarthScaleFactor;
		const float muT = muS + muA;

		//const float sunSolidAngle = 4.0f * PI * square(glm::sin(glm::radians(sunParams.AngularSizeDeg / 2.0f)));
		//const float3 sunRadiance = sunParams.Color * sunParams.Irradiance / std::max(sunSolidAngle, 1e-6f);
		const float3 sunRadiance = sunParams.Color * sunParams.Irradiance * 60.f;

		const float downscaleFactor = (float)m_RenderTargetDenom;

		// Fill shader constants
		auto* cloudsData = (interop::CloudsData*)m_CloudsCB.Map();

		cloudsData->cloudsBaseShapeTexture = cloudsShape->GetSampledView();
		cloudsData->cloudsDetailTexture = cloudsDetail->GetSampledView();
		cloudsData->linearDepthTexDI = m_RenderGraph->GetTextureSampledView(m_LinearDepthTexture);
		cloudsData->prevCloudsTexDI = m_CloudsTexture[cloudsOtherIdx]->GetSampledView();

		cloudsData->stratusWeight = cloudsParams.StratusWeight;
		cloudsData->cumulusWeight = cloudsParams.CumulusWeight;
		cloudsData->cumulonimbusWeight = cloudsParams.CumulonimbusWeight;
		cloudsData->cloudsScale = cloudsParams.CloudsScale * atmos.EarthScaleFactor;
		cloudsData->coverage = cloudsParams.CloudsCoverage;
		cloudsData->cloudFadeDistance = cloudsParams.CloudsFadeDistance;
		cloudsData->windOffset = m_CloudsOffset;
		cloudsData->cloudLayerMin = cloudsParams.CloudsLayerMin;
		cloudsData->cloudLayerMax = cloudsParams.CloudsLayerMax;
		cloudsData->toSunDirection = toSunDirection;
		cloudsData->muT = muT;
		cloudsData->muS = muS;
		cloudsData->multiScatterContribution = cloudsParams.MultiScatterContribution;
		cloudsData->multiScatterOcclusion = cloudsParams.MultiScatterOcclusion;
		cloudsData->multiScatterEccentricity = cloudsParams.MultiScatterEccentricity;
		cloudsData->albedo = muS / std::max(muT, 1e-10f);
		cloudsData->ambientStrength = cloudsParams.AmbientStrength;
		cloudsData->earthCenter = atmos.EarthCenter;
		cloudsData->earthRadius = atmos.EarthRadius;
		cloudsData->invCloudLayerThickness = 1.f / (cloudsParams.CloudsLayerMax - cloudsParams.CloudsLayerMin);
		cloudsData->cameraForward = GetCamera()->GetForward();
		cloudsData->detailScale = cloudsParams.DetailScale * atmos.EarthScaleFactor;
		cloudsData->detailErosionStrength = cloudsParams.DetailErosionStrength;
		cloudsData->matPrevFrameViewProj = GetRenderView()->GetPrevFrameViewProjMatrix();
		cloudsData->invCloudFadeDistance = 1.f / cloudsParams.CloudsFadeDistance;
		cloudsData->sunRadiance = sunRadiance;
		cloudsData->sunIrradiance = float3{ 0.f }; // TODO
		cloudsData->maxSteps = m_Params.CloudRaymarchIterations;
		cloudsData->volumetricShadows = (uint32_t)m_Params.VolumetricShadows;
		cloudsData->lightSteps = m_Params.LightRaymarchIterations;
		cloudsData->multiScatterOctaves = m_Params.MultiScatterOctaves;
		cloudsData->phaseGForward = cloudsParams.PhaseGForward;
		cloudsData->phaseGBackward = cloudsParams.PhaseGBackward;
		cloudsData->multiScatterBaseG = cloudsParams.MultiScatterBaseG;
		cloudsData->powderStrength = cloudsParams.PowderStrength;
		cloudsData->powderEdgeWidth = cloudsParams.PowderEdgeWidth;
		cloudsData->depthThreshold = 0.05f * downscaleFactor;
		cloudsData->blendFactor = 0.4f / downscaleFactor;

		const float3 up = abs(toSunDirection.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
		cloudsData->sunT = normalize(cross(toSunDirection, up));
		cloudsData->sunB = cross(toSunDirection, cloudsData->sunT);

		interop::CloudsConstants cloudsConstants;
		cloudsConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
		cloudsConstants.cameraPosition = GetCamera()->GetPosition();
		cloudsConstants.cloudsDataDI = m_CloudsCB.GetUniformView();
		cloudsConstants.viewportSize = { m_CloudsFB[m_CloudsTextureIdx]->GetFramebufferInfo().width,
			m_CloudsFB[m_CloudsTextureIdx]->GetFramebufferInfo().height };
		cloudsConstants.frameCounter = m_RenderGraph->GetDeviceManager()->GetFrameIndex();
		cloudsConstants.debugChannel = (uint32_t)m_DebugChannel;

		commandList->PushGraphicsConstants(0, cloudsConstants);

		commandList->Draw(3);

		commandList->EndRenderPass();
	}

	commandList->PushBarrier(rhi::Barrier::Texture(
		m_CloudsTexture[m_CloudsTextureIdx].get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::SHADER_RESOURCE));
	m_CloudsTextureState[m_CloudsTextureIdx] = rhi::ResourceState::SHADER_RESOURCE;

	// Composite pass
	{
		commandList->BeginRenderPass(
			m_RenderGraph->GetFrameBuffer(m_CompositeFB).get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_CompositePSO.get());

		interop::BlitGraphicsConstants shaderConstants;
		shaderConstants.textureDI = m_CloudsTexture[m_CloudsTextureIdx]->GetSampledView();

		commandList->PushGraphicsConstants(0, shaderConstants);
		commandList->Draw(3);

		commandList->EndRenderPass();
	}
}

void alm::gfx::CloudsRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* shaderFactory = deviceManager->GetShaderFactory();

	m_CloudsPS = shaderFactory->LoadShader("Clouds_ps", rhi::ShaderType::Pixel);

	ResetCloudsResources();

	// Composite
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = {
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::One,
			.destBlend = rhi::BlendFactor::SrcAlpha,
			.blendOp = rhi::BlendOp::Add };

		rhi::GraphicsPipelineStateDesc psoDesc{
			.VS = commonResources->GetBlitVS(),
			.PS = commonResources->GetBlitPS(),
			.blendState = blendState };

		m_CompositePSO = device->CreateGraphicsPipelineState(psoDesc, m_RenderGraph->GetFrameBuffer(m_CompositeFB)->GetFramebufferInfo(), "CloudsRS_Composite");
	}

	m_CloudsCB.InitUniformBuffer(sizeof(interop::CloudsData), deviceManager, "CloudsConstantBuffer");
}

void alm::gfx::CloudsRenderStage::OnDetached()
{
	m_CloudsFB[0].reset();
	m_CloudsFB[1].reset();
	m_CloudsTexture[0].reset();
	m_CloudsTexture[1].reset();
	m_CloudsCB.Release();
	m_CloudsPSO.reset();
	m_CloudsPS.reset();
	m_CompositePSO.reset();
}

void alm::gfx::CloudsRenderStage::OnBackbufferResize()
{
	ResetCloudsResources();
}

void alm::gfx::CloudsRenderStage::SetRenderTargetDenominator(int v)
{
	if (m_RenderTargetDenom == v)
		return;

	m_RenderTargetDenom = v;
	ResetCloudsResources();
}

void alm::gfx::CloudsRenderStage::ResetCloudsResources()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* device = deviceManager->GetDevice();
	const auto& bbDesc = GetRenderView()->GetBackBuffer()->GetDesc();

	// Textures and framebuffers
	{
		rhi::TextureDesc desc = {
			.width = bbDesc.width / m_RenderTargetDenom,
			.height = bbDesc.height / m_RenderTargetDenom,
			.format = rhi::Format::RGBA16_FLOAT,
			.shaderUsage = rhi::TextureShaderUsage::Sampled | rhi::TextureShaderUsage::ColorTarget };

		m_CloudsTexture[0] = device->CreateTexture(desc, rhi::ResourceState::RENDERTARGET, "CloudsTexture[0]");
		m_CloudsTextureState[0] = rhi::ResourceState::RENDERTARGET;
		m_CloudsTexture[1] = device->CreateTexture(desc, rhi::ResourceState::RENDERTARGET, "CloudsTexture[1]");
		m_CloudsTextureState[1] = rhi::ResourceState::RENDERTARGET;

		m_CloudsFB[0] = device->CreateFramebuffer(rhi::FramebufferDesc()
			.AddColorAttachment(m_CloudsTexture[0].get_weak()), "CloudsFB[0]");
		m_CloudsFB[1] = device->CreateFramebuffer(rhi::FramebufferDesc()
			.AddColorAttachment(m_CloudsTexture[1].get_weak()), "CloudsFB[1]");
	}
	m_CloudsTextureIdx = -1;

	// Clouds PSO
	{
		rhi::DepthStencilState depthStencilState{
			.depthTestEnable = false,
			.depthFunc = rhi::ComparisonFunc::GreaterEqual };

		rhi::GraphicsPipelineStateDesc psoDesc{
			.VS = commonResources->GetBlitVS(),
			.PS = m_CloudsPS.get_weak(),
			.depthStencilState = depthStencilState };

		m_CloudsPSO = device->CreateGraphicsPipelineState(psoDesc, m_CloudsFB[0]->GetFramebufferInfo(), "CloudsRS");
	}
}