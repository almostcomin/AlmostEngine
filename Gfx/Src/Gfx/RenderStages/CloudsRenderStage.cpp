#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderView.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Gfx/AtmosphereConfig.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

alm::gfx::CloudsRenderStage::CloudsRenderStage() : m_CloudsTextureIdx{ -1 }, m_DebugChannel{ DebugChannel::Disabled }
{}

void alm::gfx::CloudsRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_LinearDepthTexture = builder.GetTextureHandle("LinearDepth");
	m_CloudsTexture[0] = builder.CreateTexture("CloudsTexture[0]", RenderGraph::TextureResourceType::ShaderResource,
		RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), 1,
		rhi::Format::RGBA16_FLOAT, true);
	m_CloudsTexture[1] = builder.CreateTexture("CloudsTexture[1]", RenderGraph::TextureResourceType::ShaderResource,
		RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), 1,
		rhi::Format::RGBA16_FLOAT, true);

	m_CompositeFB = builder.RequestFramebuffer({ m_SceneColorTexture });

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_LinearDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_CloudsTexture[0], RenderGraph::AccessMode::Write,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_CloudsTexture[1], RenderGraph::AccessMode::Write,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::CloudsRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!GetScene())
		return;
	if (!GetCamera())
		return;
	
	gfx::AtmosphereConfig* atmos = GetScene()->GetAtmosphereConfig();
	if (!atmos->CloudsSubsystemInitialized())
		return;

	if (!atmos)
		return;

	rhi::TextureHandle cloudsShape = atmos->GetCloudsShapeTexture();
	rhi::TextureHandle cloudsDetail = atmos->GetCloudsDetailTexture();
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

	uint2 cloudsTexDims = m_RenderGraph->GetTexture2dDimensions(m_CloudsTexture[m_CloudsTextureIdx]);

	// Clear texture if requested
	if (clearCloudsTextures)
	{
		auto* commonResources = GetDeviceManager()->GetCommonResources();
		commandList->SetPipelineState(commonResources->GetClearTexturePSO().get());

		commandList->PushBarrier(rhi::Barrier::Texture(m_RenderGraph->GetTexture(m_CloudsTexture[cloudsOtherIdx]).get(),
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS));

		interop::ClearTextureConstants shaderConstants;
		shaderConstants.textureDI = m_RenderGraph->GetTextureStorageView(m_CloudsTexture[cloudsOtherIdx]);
		shaderConstants.textureDim = float2{ cloudsTexDims.x, cloudsTexDims.y };
		shaderConstants.clearValue = float4{ 0.f, 0.f, 0.f, 1.f };

		commandList->PushComputeConstants(0, shaderConstants);
		commandList->Dispatch(DivRoundUp(cloudsTexDims.x, 16u), DivRoundUp(cloudsTexDims.y, 16u), 1);

		commandList->PushBarrier(rhi::Barrier::Texture(m_RenderGraph->GetTexture(m_CloudsTexture[cloudsOtherIdx]).get(),
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));
	}

	// Transitions
	commandList->PushBarrier(rhi::Barrier::Texture(m_RenderGraph->GetTexture(m_CloudsTexture[m_CloudsTextureIdx]).get(),
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS));

	// Render clouds pass
	{
		commandList->SetPipelineState(m_CloudsPSO.get());

		const gfx::AtmosphereConfig::SunParams& sunParams = atmos->Sun;
		const gfx::AtmosphereConfig::CloudsParams& cloudsParams = atmos->Clouds;

		const float3 toSunDirection = -glm::normalize(alm::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg)));
		const float muS = cloudsParams.ScatteringCoeff / atmos->EarthScaleFactor;
		const float muA = cloudsParams.AbsorptionCoeff / atmos->EarthScaleFactor;
		const float muT = muS + muA;

		//const float sunSolidAngle = 4.0f * PI * square(glm::sin(glm::radians(sunParams.AngularSizeDeg / 2.0f)));
		//const float3 sunRadiance = sunParams.Color * sunParams.Irradiance / std::max(sunSolidAngle, 1e-6f);
		const float3 sunRadiance = sunParams.Color * sunParams.Irradiance * 60.f;

		const float downscaleFactor = (float)m_RenderTargetDenom;

		// Fill shader constants
		auto* cloudsData = (interop::CloudsData*)m_CloudsCB.Map();

		cloudsData->DstTextureDI = m_RenderGraph->GetTextureStorageView(m_CloudsTexture[m_CloudsTextureIdx]);
		cloudsData->DstTextureSize = cloudsTexDims;

		cloudsData->linearDepthTexDI = m_RenderGraph->GetTextureSampledView(m_LinearDepthTexture);
		cloudsData->prevCloudsTexDI = m_RenderGraph->GetTextureSampledView(m_CloudsTexture[cloudsOtherIdx]);

		cloudsData->cloudFadeDistance = cloudsParams.CloudsFadeDistance;
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
		cloudsData->earthCenter = atmos->EarthCenter;
		cloudsData->earthRadius = atmos->EarthRadius;
		cloudsData->invCloudLayerThickness = 1.f / (cloudsParams.CloudsLayerMax - cloudsParams.CloudsLayerMin);
		cloudsData->cameraForward = GetCamera()->GetForward();
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
		cloudsConstants.viewportSize = cloudsTexDims;
		cloudsConstants.frameCounter = m_RenderGraph->GetDeviceManager()->GetFrameIndex();
		cloudsConstants.debugChannel = (uint32_t)m_DebugChannel;
		cloudsConstants.cloudsShapeDataDI = atmos->GetCloudsShapeUniformView();

		commandList->PushComputeConstants(0, cloudsConstants);
		commandList->Dispatch(DivRoundUp(cloudsTexDims.x, 16u), DivRoundUp(cloudsTexDims.y, 16u), 1);
	}

	commandList->PushBarrier(rhi::Barrier::Texture(m_RenderGraph->GetTexture(m_CloudsTexture[m_CloudsTextureIdx]).get(),
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

	// Composite pass
	{
		commandList->BeginRenderPass(
			m_RenderGraph->GetFrameBuffer(m_CompositeFB).get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_CompositePSO.get());

		interop::BlitGraphicsConstants shaderConstants;
		shaderConstants.textureDI = m_RenderGraph->GetTextureSampledView(m_CloudsTexture[m_CloudsTextureIdx]);

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

	m_CloudsCS = shaderFactory->LoadShader("Clouds_cs", rhi::ShaderType::Compute);

	// Clouds PSO
	{
		rhi::ComputePipelineStateDesc psoDesc{
			.CS = m_CloudsCS.get_weak() };

		m_CloudsPSO = device->CreateComputePipelineState(psoDesc, "CloudsRS");
	}

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
	m_CloudsCB.Release();
	m_CloudsPSO.reset();
	m_CloudsCS.reset();
	m_CompositePSO.reset();
}

void alm::gfx::CloudsRenderStage::OnBackbufferResize()
{
	m_CloudsTextureIdx = -1;
}

void alm::gfx::CloudsRenderStage::SetRenderTargetDenominator(int v)
{
	if (m_RenderTargetDenom == v)
		return;

	m_RenderTargetDenom = v;
	
	m_RenderGraph->RecreateTexture(m_CloudsTexture[0],
		RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), 1,
		rhi::Format::RGBA16_FLOAT);
	m_RenderGraph->RecreateTexture(m_CloudsTexture[1],
		RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), RenderGraph::GetBackBufferSizeDenominator(m_RenderTargetDenom), 1,
		rhi::Format::RGBA16_FLOAT);

	m_CloudsTextureIdx = -1;
}