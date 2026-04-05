#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
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

#define NUM_CELLS 16.0f

namespace
{
	// Arbitrary random
	float rand(float2 st)
	{
		float dotVal = st.x * 12.9898f + st.y * 78.233f;
		float sinVal = std::sin(dotVal);
		float fracPart = sinVal * 43758.5453f;
		// frac(x) = x - floor(x)
		return fracPart - std::floor(fracPart);
	}

	// Performs worley noise by checking all adjacent cells and comparing the distance to their points
	float WorleyNoise(const float2& st) 
	{
		auto getCellPoint = [](const float2& cell)
		{
			float2 cell_base = cell / NUM_CELLS;
			float noise_x = rand(cell);
			float noise_y = rand(float2{ cell.y, cell.x });
			return cell_base + (0.5f + 1.5f * float2{ noise_x, noise_y }) / NUM_CELLS;
		};

		int2 cell = int2{ st * NUM_CELLS };
		float dist = 1.0;

		// Search in the surrounding 5x5 cell block
		for (int x = 0; x < 5; x++) 
		{
			for (int y = 0; y < 5; y++) 
			{
				float2 cell_point = getCellPoint(cell + int2(x - 2, y - 2));
				dist = std::min(dist, glm::distance(cell_point, st));
			}
		}

		dist /= glm::length(float2(1.0 / NUM_CELLS));
		dist = 1.0 - dist;
		return dist;
	}
} // anonymous namespace

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

	const auto& sunParams = GetScene()->GetSunParams();

	m_Params.Offset += GetRenderView()->GetTimeDelta();

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{}, rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::SkyConstants shaderConstants;
	shaderConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
	shaderConstants.windVelocity = float2{ 0.1, 0.05 };
	shaderConstants.cloudScale = 0.002f;
	shaderConstants.sunDirection = alm::ElevationAzimuthRadToDir(
		glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));
	shaderConstants.time = GetRenderView()->GetTime() * 1.0;
	shaderConstants.cloudBaseShapeTexture = m_CloudBaseShapeTexture->GetSampledView();

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

	CreateNoiseTextures();
}

void alm::gfx::SkyRenderStage::OnDetached()
{
	m_CloudBaseShapeTexture.reset();
	m_PSO.reset();
	m_PS.reset();
}

void alm::gfx::SkyRenderStage::CreateNoiseTextures()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* dataUploader = deviceManager->GetDataUploader();

	rhi::TextureDesc desc{
		.width = 128,
		.height = 128,
		.depth = 128,
		.format = rhi::Format::RGBA8_UNORM,
		.dimension = rhi::TextureDimension::Texture3D,
		.shaderUsage = rhi::TextureShaderUsage::Sampled };
	
	m_CloudBaseShapeTexture = device->CreateTexture(desc, rhi::ResourceState::COPY_DST, "CloudBaseShapeTexture");

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
				float2 st = float2{ x, y } / 128.f;

				st.x += z / 128.f;

				float w = WorleyNoise(st);
				byte v = (byte)(w * 255.f);
				*data = MakeRGBA(v, v, v, v);
				++data;
			}
		}
	}

	auto commitResult = dataUploader->CommitUploadTextureTicket(std::move(ticket), m_CloudBaseShapeTexture.get_weak(), rhi::ResourceState::COPY_DST,
		rhi::ResourceState::SHADER_RESOURCE);
	assert(commitResult);
	commitResult->Wait();
}