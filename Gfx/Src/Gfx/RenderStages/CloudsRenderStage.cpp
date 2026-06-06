#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Gfx/RenderView.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Gfx/DataUploader.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"
#include "Core/Noise.h"

alm::gfx::CloudsRenderStage::CloudsRenderStage() : m_CloudsTextureIdx{ -1 }
{}

std::expected<std::pair<alm::rhi::TextureOwner, alm::SignalListener>, std::string>
alm::gfx::CloudsRenderStage::CreateCloudsShapeTexture(alm::gfx::DeviceManager* deviceManager)
{
	auto* device = deviceManager->GetDevice();
	auto* dataUploader = deviceManager->GetDataUploader();

	rhi::TextureDesc desc{
		.width = 128,
		.height = 128,
		.depth = 128,
		.format = rhi::Format::RGBA8_UNORM,
		.dimension = rhi::TextureDimension::Texture3D,
		.shaderUsage = rhi::TextureShaderUsage::Sampled };

	alm::rhi::TextureOwner texture = device->CreateTexture(desc, rhi::ResourceState::COPY_DST, "CloudsShapeTexture");

	auto requestResult = dataUploader->RequestUploadTicket(desc, rhi::AllSubresources);
	assert(requestResult);
	auto& ticket = *requestResult;
	assert(ticket.GetSize() == 128 * 128 * 128 * 4);

	for (int z = 0; z < 128; ++z)
	{
		uint32_t* data = (uint32_t*)ticket.GetPtr();
		data += 128 * 128 * z;
		for (int y = 0; y < 128; ++y)
		{
			for (int x = 0; x < 128; ++x)
			{
				float3 st = float3{ x, y, z } / 128.f;
				float3 stG = st;
				float3 stB = st + float3(0.5f, 0.5f, 0.5f);
				float3 stA = st + float3(0.25f, 0.75f, 0.5f);

				float g = WorleyNoise(stG * 4.f, 4.f);
				float b = WorleyNoise(stB * 9.f, 9.f);
				float a = WorleyNoise(stA * 19.f, 19.f);

				float pfbm = PerlinFbm(st, 4.f, 7);
				pfbm = std::lerp(pfbm, 1.0f, 0.5f);

				float perlin = std::lerp(1.0f, pfbm, 0.9f);
				float worley = std::lerp(1.0f, g, 0.7f);
				float r = perlin * worley;

				*data = MakeRGBA(r * 255.f, g * 255.f, b * 255.f, a * 255.f);
				++data;
			}
		}
	}

	auto commitResult = dataUploader->CommitUploadTextureTicket(std::move(ticket), texture.get_weak(),
		rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
	if (!commitResult)
	{
		return std::unexpected(commitResult.error());
	}

	return std::make_pair(std::move(texture), *commitResult);
}

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
	return;

	if (!GetScene())
		return;
	if (!GetCamera())
		return;
	if (!m_CloudsShapeTexture)
	{
		LOG_WARNING("CloudsRenderStage: No clouds texture defined");
		return;
	}

	const auto& sunParams = GetScene()->GetSunParams();

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

		// Update clouds position offset
		m_CloudsOffset += m_Params.WindVelocity * GetRenderView()->GetTimeDelta();

		// Fill shader constants
		auto* cloudsData = (interop::CloudsData*)m_CloudsCB.Map();

		cloudsData->cloudBaseShapeTexture = m_CloudsShapeTexture->GetSampledView();
		cloudsData->cloudsScale = m_Params.CloudsScale;
		cloudsData->coverage = m_Params.CloudsCoverage;
		cloudsData->cloudFadeDistance = m_Params.CloudsFadeDistance;
		cloudsData->windOffset = m_CloudsOffset;
		cloudsData->cloudLayerMin = m_Params.CloudsLayerMin;
		cloudsData->cloudLayerMax = m_Params.CloudsLayerMax;
		cloudsData->toSunDirection = -glm::normalize(alm::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg)));
		cloudsData->absorptionCoeff = m_Params.AbsorptionCoeff;
		cloudsData->earthCenter = float3(0.f, -m_Params.EarthRadius, 0.f);// GetCamera()->GetPosition() + float3(0.f, -m_Params.EarthRadius, 0.f);
		cloudsData->earthRadius = m_Params.EarthRadius;
		cloudsData->invCloudLayerThickness = 1.f / (m_Params.CloudsLayerMax - m_Params.CloudsLayerMin);
		cloudsData->maxSteps = m_Params.CloudRaymarchIterations;
		cloudsData->lightSteps = m_Params.LightRaymarchIterations;
		cloudsData->linearDepthTexDI = m_RenderGraph->GetTextureSampledView(m_LinearDepthTexture);
		cloudsData->cameraForward = GetCamera()->GetForward();
		cloudsData->prevCloudsTexDI = m_CloudsTexture[cloudsOtherIdx]->GetSampledView();
		cloudsData->matPrevFrameViewProj = GetRenderView()->GetPrevFrameViewProjMatrix();

		interop::CloudsConstants cloudsConstants;
		cloudsConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
		cloudsConstants.cameraPosition = GetCamera()->GetPosition();
		cloudsConstants.cloudsDataDI = m_CloudsCB.GetUniformView();
		cloudsConstants.time = GetRenderView()->GetTime() * 1.0;

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

void alm::gfx::CloudsRenderStage::ResetCloudsResources()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* device = deviceManager->GetDevice();
	const auto& bbDesc = GetRenderView()->GetBackBuffer()->GetDesc();

	// Textures and framebuffers
	{
		rhi::TextureDesc desc = {
			.width = bbDesc.width,
			.height = bbDesc.height,
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