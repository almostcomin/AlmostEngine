#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/BloomRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

alm::gfx::BloomRenderStage::BloomRenderStage() : 
	m_BloomEnabled{ true },
	m_FilterRadius{ 0.005f },
	m_Strength{ 0.04f },
	m_Threshold{ 1.f },
	m_Knee{ 0.5f },
	m_MipChainLength { 7 }
{}

void alm::gfx::BloomRenderStage::SetMaxMipChainLenght(uint32_t v) 
{ 
	if (v != m_MipChainLength)
	{
		m_MipChainLength = v;
		ResetMipChain(false);
	}
}

void alm::gfx::BloomRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_BloomPrefilterTexture = builder.CreateTexture("BloomPrefilter", RenderGraph::TextureResourceType::ShaderResource, 
		RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);
	m_BloomResultTexture = builder.CreateColorTarget("BloomResult", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);

	m_FB = builder.RequestFramebuffer({ m_BloomResultTexture }, nullptr);

	builder.AddTextureDependency(m_BloomPrefilterTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_BloomResultTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::BloomRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!m_BloomEnabled)
	{
		rhi::ITexture* dstTex = m_RenderGraph->GetTexture(m_BloomResultTexture).get();
		rhi::ITexture* srcTex = m_RenderGraph->GetTexture(m_SceneColorTexture).get();
		rhi::ITexture* preFilterTex = m_RenderGraph->GetTexture(m_BloomPrefilterTexture).get();

		commandList->PushBarriers({
			rhi::Barrier::Texture(dstTex, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::COPY_DST),
			rhi::Barrier::Texture(srcTex, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_SRC) });

		// PassThrough
		commandList->CopyTextureToTexture(dstTex, srcTex);

		commandList->PushBarriers({
			rhi::Barrier::Texture(dstTex, rhi::ResourceState::COPY_DST, rhi::ResourceState::RENDERTARGET),
			rhi::Barrier::Texture(srcTex, rhi::ResourceState::COPY_SRC, rhi::ResourceState::SHADER_RESOURCE),
			rhi::Barrier::Texture(preFilterTex, rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE) });

		return;
	}

	rhi::TextureHandle sceneColorTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);
	const auto sceneColorTexDesc = sceneColorTexture->GetDesc();
	rhi::TextureHandle bloomPrefilterTexture = m_RenderGraph->GetTexture(m_BloomPrefilterTexture);

	// Filter
	{
		commandList->BeginMarker("Downsample");
		commandList->SetPipelineState(m_FilterPSO.get());

		interop::BloomPrefilterConstants shaderConstants;
		shaderConstants.inputTextureDI = sceneColorTexture->GetSampledView();
		shaderConstants.outputTextureDI = bloomPrefilterTexture->GetStorageView();
		shaderConstants.texResolution = uint2{ sceneColorTexDesc.width, sceneColorTexDesc.height };
		shaderConstants.invTexResolution = 1.f / float2{ sceneColorTexDesc.width, sceneColorTexDesc.height };
		shaderConstants.threshold = m_Threshold;
		shaderConstants.knee = m_Knee;

		commandList->PushComputeConstants(0, shaderConstants);

		commandList->Dispatch(DivRoundUp(sceneColorTexDesc.width, 16u), DivRoundUp(sceneColorTexDesc.height, 16u), 1);

		commandList->PushBarrier(rhi::Barrier::Texture(bloomPrefilterTexture.get(),
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

		commandList->EndMarker();
	}

	// Progressively downsample through the mip chain
	commandList->BeginMarker("Downsample");
	commandList->SetPipelineState(m_DownsamplePSO.get());

	for (uint32_t i = 0; i < m_MipChain.size(); i++)
	{
		const bool firstDownsample = (i == 0);

		rhi::ITexture* inputTex = firstDownsample ?
			inputTex = bloomPrefilterTexture.get() : m_MipChain[i - 1].get();
		const auto& outputTexDesc = m_MipChain[i]->GetDesc();
		const auto& inputTexDesc = inputTex->GetDesc();

		interop::BloomDownsampleConstants shaderConstants;
		shaderConstants.inputTextureDI = inputTex->GetSampledView();
		shaderConstants.outputTextureDI = m_MipChain[i]->GetStorageView();
		shaderConstants.outputTexResolution = uint2{ outputTexDesc.width, outputTexDesc.height };
		shaderConstants.inputTexInvResolution = 1.f / float2{ inputTexDesc.width, inputTexDesc.height };

		commandList->PushComputeConstants(0, shaderConstants);

		if (!firstDownsample)
		{
			commandList->PushBarrier(rhi::Barrier::Texture(inputTex,
				rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));
		}

		commandList->Dispatch(DivRoundUp(outputTexDesc.width, 16u), DivRoundUp(outputTexDesc.height, 16u), 1);
	}
	commandList->EndMarker();

	// Upsample
	commandList->BeginMarker("Upsample");
	commandList->SetPipelineState(m_UpsamplePSO.get());

	for (int i = m_MipChain.size() - 1; i > 0; --i)
	{
		const auto& outputTexDesc = m_MipChain[i - 1]->GetDesc();

		std::vector<rhi::Barrier> barriers;
		barriers.reserve(2);
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i].get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i - 1].get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS));
		commandList->PushBarriers(barriers);

		interop::BloomUpsampleConstants shaderConstants;
		shaderConstants.inputTextureDI = m_MipChain[i]->GetSampledView();
		shaderConstants.outputTextureDI = m_MipChain[i - 1]->GetStorageView();
		shaderConstants.outputTexInvResolution = 1.f / float2{ outputTexDesc.width, outputTexDesc.height };
		shaderConstants.outputTexResolution = uint2{ outputTexDesc.width, outputTexDesc.height };
		shaderConstants.filterRadius = m_FilterRadius * (1080.f / sceneColorTexDesc.height);

		commandList->PushComputeConstants(0, shaderConstants);

		commandList->Dispatch(DivRoundUp(outputTexDesc.width, 16u), DivRoundUp(outputTexDesc.height, 16u), 1);
	}
	commandList->EndMarker();

	// Final mix
	commandList->BeginMarker("Mix");
	{
		commandList->PushBarrier(rhi::Barrier::Texture(
			m_MipChain[0].get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::SHADER_RESOURCE));

		commandList->BeginRenderPass(m_RenderGraph->GetFrameBuffer(m_FB).get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Discard, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_MixPSO.get());

		interop::BloomMixConstants shaderConstants;
		shaderConstants.sceneTextureDI = sceneColorTexture->GetSampledView();
		shaderConstants.bloomTextureDI = m_MipChain[0]->GetSampledView();
		shaderConstants.bloomStrength = m_Strength;

		commandList->PushGraphicsConstants(0, shaderConstants);

		commandList->Draw(3);

		commandList->EndRenderPass();
	}
	commandList->EndMarker();

	// After finished all mip textures are in SHADER_RESOURCE state
	// We need to transit them to UNORDERED_ACCESS so they are ready for the next frame
	std::vector<rhi::Barrier> barriers;
	barriers.reserve(m_MipChain.size());
	for (int i = 0; i < m_MipChain.size(); ++i)
	{
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i].get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::UNORDERED_ACCESS));
	}
	commandList->PushBarriers(barriers);
}

void alm::gfx::BloomRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* shaderFactory = deviceManager->GetShaderFactory();
	auto* device = deviceManager->GetDevice();
	rhi::TextureHandle sceneTex = m_RenderGraph->GetTexture(m_SceneColorTexture);
	rhi::TextureHandle bloomTex = m_RenderGraph->GetTexture(m_BloomResultTexture);

	// Create shaders
	{
		m_DownsampleShader = shaderFactory->LoadShader("BloomDownsample_cs", rhi::ShaderType::Compute);
		m_UpsampleShader = shaderFactory->LoadShader("BloomUpsample_cs", rhi::ShaderType::Compute);
		m_MixShader = shaderFactory->LoadShader("BloomMix_ps", rhi::ShaderType::Pixel);
		m_FilterShader = shaderFactory->LoadShader("BloomPrefilter_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_MixPSO = commonResources->CreateFullscreenPassPSO(
			m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(),
			m_MixShader.get_weak(), "BloomMixPSO");

		m_DownsamplePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ .CS = m_DownsampleShader.get_weak() }, "BloomDownsamplePSO");

		m_UpsamplePSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ .CS = m_UpsampleShader.get_weak() }, "BloomUpsamplePSO");

		m_FilterPSO = device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ .CS = m_FilterShader.get_weak() }, "BloomFilterPSO");
	}

	// Create texture mips
	ResetMipChain(false);
}

void alm::gfx::BloomRenderStage::OnDetached()
{
	ReleaseMipChain(false);

	m_FilterPSO = {};
	m_UpsamplePSO = {};
	m_DownsamplePSO = {};
	m_MixPSO = {};

	m_FilterShader = {};
	m_MixShader = {};
	m_UpsampleShader = {};
	m_DownsampleShader = {};
}

void alm::gfx::BloomRenderStage::OnBackbufferResize()
{
	ResetMipChain(true);
}

void alm::gfx::BloomRenderStage::ReleaseMipChain(bool immediate)
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();

	for (auto& mip : m_MipChain)
	{
		if (immediate)
		{
			device->ReleaseImmediately(std::move(mip));
		}
		else
		{
			device->ReleaseQueued(std::move(mip));
		}
	}
	m_MipChain.clear();
}

void alm::gfx::BloomRenderStage::ResetMipChain(bool immediate)
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* commonResources = deviceManager->GetCommonResources();
	rhi::TextureHandle sceneTex = m_RenderGraph->GetTexture(m_SceneColorTexture);
	const auto& sceneTexDesc = sceneTex->GetDesc();

	ReleaseMipChain(immediate);

	// Create texture mips / framebuffers
	uint32_t width = sceneTexDesc.width;
	uint32_t height = sceneTexDesc.height;
	for (int i = 0; i < m_MipChainLength; ++i)
	{
		width /= 2; height /= 2;
		if (width == 0 || height == 0)
			break;

		rhi::TextureDesc desc = {};
		desc.width = width;
		desc.height = height;
		desc.format = sceneTexDesc.format;
		desc.shaderUsage = rhi::TextureShaderUsage::Sampled | rhi::TextureShaderUsage::Storage;

		auto mipTex = device->CreateTexture(desc, rhi::ResourceState::UNORDERED_ACCESS, std::format("BloomMip[{}]", i));

		m_MipChain.push_back(std::move(mipTex));
	}
}