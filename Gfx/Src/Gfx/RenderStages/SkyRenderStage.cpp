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

namespace
{
	// Hash by David_Hoskins
	constexpr uint32_t UI0 = 1597334673;
	constexpr uint32_t UI1 = 3812015801;
	constexpr uint2 UI2 = uint2{ UI0, UI1 };
	constexpr uint3 UI3 = uint3{ UI0, UI1, 2798796415 };
	constexpr float UIF = 1.0f / float(0xffffffffU);

	float3 hash33(float3 p)
	{
		uint3 q = uint3(int3(p)) * UI3;
		q = (q.x ^ q.y ^ q.z) * UI3;
		return -1.0f + 2.0f * float3(q) * UIF;
	}

	// Gradient noise by iq (modified to be tileable)
	float GradientNoise(const float3& x, float freq)
	{
		// grid
		float3 p = floor(x);
		float3 w = fract(x);

		// quintic interpolant
		float3 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);

		// gradients
		float3 ga = hash33(mod(p + float3(0.f, 0.f, 0.f), freq));
		float3 gb = hash33(mod(p + float3(1.f, 0.f, 0.f), freq));
		float3 gc = hash33(mod(p + float3(0.f, 1.f, 0.f), freq));
		float3 gd = hash33(mod(p + float3(1.f, 1.f, 0.f), freq));
		float3 ge = hash33(mod(p + float3(0.f, 0.f, 1.f), freq));
		float3 gf = hash33(mod(p + float3(1.f, 0.f, 1.f), freq));
		float3 gg = hash33(mod(p + float3(0.f, 1.f, 1.f), freq));
		float3 gh = hash33(mod(p + float3(1.f, 1.f, 1.f), freq));

		// projections
		float va = dot(ga, w - float3(0.f, 0.f, 0.f));
		float vb = dot(gb, w - float3(1.f, 0.f, 0.f));
		float vc = dot(gc, w - float3(0.f, 1.f, 0.f));
		float vd = dot(gd, w - float3(1.f, 1.f, 0.f));
		float ve = dot(ge, w - float3(0.f, 0.f, 1.f));
		float vf = dot(gf, w - float3(1.f, 0.f, 1.f));
		float vg = dot(gg, w - float3(0.f, 1.f, 1.f));
		float vh = dot(gh, w - float3(1.f, 1.f, 1.f));

		// interpolation
		return va +
			u.x * (vb - va) +
			u.y * (vc - va) +
			u.z * (ve - va) +
			u.x * u.y * (va - vb - vc + vd) +
			u.y * u.z * (va - vc - ve + vg) +
			u.z * u.x * (va - vb - ve + vf) +
			u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
	}

	// Tileable 3D worley noise
	float WorleyNoise(const float3& uv, float freq)
	{
		float3 id = floor(uv);
		float3 p = fract(uv);

		float minDist = 10000.;
		for (float x = -1.; x <= 1.; ++x)
		{
			for (float y = -1.; y <= 1.; ++y)
			{
				for (float z = -1.; z <= 1.; ++z)
				{
					float3 offset = float3(x, y, z);
					float3 h = hash33(mod(id + offset, float3(freq)));
					h = h * 0.5f + 0.5f;
					h += offset;
					float3 d = p - h;
					minDist = std::min(minDist, dot(d, d));
				}
			}
		}

		return 1.0f - alm::Saturate(minDist);
	}

	// Fbm for Perlin noise based on iq's blog
	float PerlinFbm(const float3& p, float freq, int octaves)
	{
		float G = exp2(-.85);
		float amp = 1.;
		float noise = 0.;
		for (int i = 0; i < octaves; ++i)
		{
			noise += amp * GradientNoise(p * freq, freq);
			freq *= 2.;
			amp *= G;
		}

		return noise;
	}

} // anonymous namespace

std::expected<std::pair<alm::rhi::TextureOwner, alm::SignalListener>, std::string>
alm::gfx::SkyRenderStage::CreateCloudsShapeTexture(alm::gfx::DeviceManager* deviceManager)
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

	if (!m_CloudsShapeTexture)
		return;

	const auto& sunParams = GetScene()->GetSunParams();

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{}, rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	auto* cloudsConstants = (interop::CloudsData*)m_CloudsCB.Map();
	cloudsConstants->windVelocity = m_Params.WindVelocity;
	cloudsConstants->cloudScale = m_Params.CloudsScale;
	cloudsConstants->coverage = m_Params.CloudsCoverage;
	cloudsConstants->cloudLayerMin = m_Params.CloudsLayerMin;
	cloudsConstants->cloudLayerMax = m_Params.CloudsLayerMax;
	cloudsConstants->absorptionCoeff = m_Params.AbsorptionCoeff;
	cloudsConstants->maxSteps = m_Params.CloudRaymarchIterations;
	cloudsConstants->toSunDirection = -glm::normalize(alm::ElevationAzimuthRadToDir(
		glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg)));
	cloudsConstants->lightSteps = m_Params.LightRaymarchIterations;
	cloudsConstants->cloudBaseShapeTexture = m_CloudsShapeTexture->GetSampledView();
	cloudsConstants->cloudFadeDistance = m_Params.CloudsFadeDistance;

	interop::SkyConstants shaderConstants;
	shaderConstants.matClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
	shaderConstants.cloudsDataDI = m_CloudsCB.GetUniformView();
	shaderConstants.time = GetRenderView()->GetTime() * 1.0;

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

	m_CloudsCB.InitUniformBuffer(sizeof(interop::CloudsData), deviceManager, "CloudsConstantBuffer");
}

void alm::gfx::SkyRenderStage::OnDetached()
{
	m_CloudsCB.Release();
	m_PSO.reset();
	m_PS.reset();
}
