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
	m_MipChainLength { 4 }
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
	m_BloomResultTexture = builder.CreateColorTarget("BloomResult", RenderGraph::c_BBSize, RenderGraph::c_BBSize, 1, rhi::Format::RGBA16_FLOAT);
	m_SceneColorTexture = builder.GetTextureHandle("SceneColor");
	m_FB = builder.RequestFramebuffer({ m_BloomResultTexture }, nullptr);

	builder.AddTextureDependency(m_SceneColorTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_BloomResultTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
}

void alm::gfx::BloomRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!m_BloomEnabled)
	{
		rhi::ITexture* dstTex = m_RenderGraph->GetTexture(m_BloomResultTexture).get();
		rhi::ITexture* srcTex = m_RenderGraph->GetTexture(m_SceneColorTexture).get();

		commandList->PushBarriers({
			rhi::Barrier::Texture(dstTex, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::COPY_DST),
			rhi::Barrier::Texture(srcTex, rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_SRC) });

		// PassThrough
		commandList->CopyTextureToTexture(dstTex, srcTex);

		commandList->PushBarriers({
			rhi::Barrier::Texture(dstTex, rhi::ResourceState::COPY_DST, rhi::ResourceState::RENDERTARGET),
			rhi::Barrier::Texture(srcTex, rhi::ResourceState::COPY_SRC, rhi::ResourceState::SHADER_RESOURCE) });

		return;
	}

	rhi::TextureHandle sceneColorTexture = m_RenderGraph->GetTexture(m_SceneColorTexture);

	// Progressively downsample through the mip chain
	commandList->BeginMarker("Downsample");
	for (uint32_t i = 0; i < m_MipChain.size(); i++)
	{
		interop::BloomDownsampleConstants shaderConstants;
		if (i == 0)
		{
			shaderConstants.inputTextureDI = sceneColorTexture->GetSampledView();
			shaderConstants.invInputTexResolution = 1.f / float2{ sceneColorTexture->GetDesc().width, sceneColorTexture->GetDesc().height };
		}
		else
		{
			rhi::ITexture* srcTex = m_MipChain[i - 1].Texture.get();
			shaderConstants.inputTextureDI = srcTex->GetSampledView();
			shaderConstants.invInputTexResolution = 1.f / float2{ srcTex->GetDesc().width, srcTex->GetDesc().height };
			commandList->PushBarrier(rhi::Barrier::Texture(srcTex, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::SHADER_RESOURCE));
		}

		commandList->BeginRenderPass(
			m_MipChain[i].Framebuffer.get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Discard, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_MipChain[i].PSO.get());
		commandList->PushGraphicsConstants(0, shaderConstants);

		commandList->Draw(3);

		commandList->EndRenderPass();
	}
	commandList->EndMarker();

	// Upsample
	commandList->BeginMarker("Upsample");
	for (int i = m_MipChain.size() - 1; i > 0; --i)
	{
		std::vector<rhi::Barrier> barriers;
		barriers.reserve(2);
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i].Texture.get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::SHADER_RESOURCE));
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i - 1].Texture.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::RENDERTARGET));
		commandList->PushBarriers(barriers);

		commandList->BeginRenderPass(m_MipChain[i - 1].Framebuffer.get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_MipChain[i - 1].BlendPSO.get());

		interop::BloomUpsampleConstants shaderConstants;
		shaderConstants.inputTextureDI = m_MipChain[i].Texture->GetSampledView();
		shaderConstants.filterRadius = m_FilterRadius;
		commandList->PushGraphicsConstants(0, shaderConstants);

		commandList->Draw(3);

		commandList->EndRenderPass();		
	}
	commandList->EndMarker();

	// Final mix
	commandList->BeginMarker("Mix");
	{
		commandList->PushBarrier(rhi::Barrier::Texture(
			m_MipChain[0].Texture.get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::SHADER_RESOURCE));

		commandList->BeginRenderPass(m_RenderGraph->GetFrameBuffer(m_FB).get(),
			{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Discard, rhi::RenderPassOp::StoreOp::Store } },
			{}, {}, rhi::RenderPassFlags::None);

		commandList->SetPipelineState(m_PSO.get());

		interop::BloomMixConstants shaderConstants;
		shaderConstants.sceneTextureDI = sceneColorTexture->GetSampledView();
		shaderConstants.bloomTextureDI = m_MipChain[0].Texture->GetSampledView();
		shaderConstants.bloomStrength = m_Strength;
		commandList->PushGraphicsConstants(0, shaderConstants);

		commandList->Draw(3);

		commandList->EndRenderPass();
	}
	commandList->EndMarker();

	// After finished all mip textures are in SHADER_RESOURCE state
	// We need to transit them to RENDERTARGET so they are ready for the next frame
	std::vector<rhi::Barrier> barriers;
	barriers.reserve(m_MipChain.size());
	for (int i = 0; i < m_MipChain.size(); ++i)
	{
		barriers.push_back(rhi::Barrier::Texture(
			m_MipChain[i].Texture.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::RENDERTARGET));
	}
	commandList->PushBarriers(barriers);
}

void alm::gfx::BloomRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();
	auto* commonResources = deviceManager->GetCommonResources();
	auto* shaderFactory = deviceManager->GetShaderFactory();
	rhi::TextureHandle sceneTex = m_RenderGraph->GetTexture(m_SceneColorTexture);
	const auto& sceneTexDesc = sceneTex->GetDesc();
	rhi::TextureHandle bloomTex = m_RenderGraph->GetTexture(m_BloomResultTexture);

	// Create shaders
	{
		m_DownsampleShader = shaderFactory->LoadShader("BloomDownsample_ps", rhi::ShaderType::Pixel);
		m_UpsampleShader = shaderFactory->LoadShader("BloomUpsample_ps", rhi::ShaderType::Pixel);
		m_MixShader = shaderFactory->LoadShader("BloomMix_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO desc
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState{
			.blendEnable = true,
			.srcBlend = rhi::BlendFactor::One,
			.destBlend = rhi::BlendFactor::One,
			.blendOp = rhi::BlendOp::Add };

		m_BlendPSODesc = rhi::GraphicsPipelineStateDesc{
			.VS = commonResources->GetBlitVS(),
			.PS = m_UpsampleShader.get_weak(),
			.blendState = blendState };
	}

	// Create resources for main render target
	{		
		m_PSO = commonResources->CreateFullscreenPassPSO(
			m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(),
			m_MixShader.get_weak(), "BloomMixPSO");
	}

	// Create texture mips / framebuffers
	ResetMipChain(false);
}

void alm::gfx::BloomRenderStage::OnDetached()
{
	ReleaseMipChain(false);

	m_BlendPSODesc = {};
	m_PSO.reset();

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
			device->ReleaseImmediately(std::move(mip.Texture));
			device->ReleaseImmediately(std::move(mip.Framebuffer));
			device->ReleaseImmediately(std::move(mip.PSO));
			device->ReleaseImmediately(std::move(mip.BlendPSO));
		}
		else
		{
			device->ReleaseQueued(std::move(mip.Texture));
			device->ReleaseQueued(std::move(mip.Framebuffer));
			device->ReleaseQueued(std::move(mip.PSO));
			device->ReleaseQueued(std::move(mip.BlendPSO));
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

		rhi::TextureDesc desc = sceneTexDesc;
		desc.width = width;
		desc.height = height;

		auto mipTex = device->CreateTexture(desc, rhi::ResourceState::RENDERTARGET, std::format("BloomMip[{}]", i));

		auto fb = device->CreateFramebuffer(rhi::FramebufferDesc()
			.AddColorAttachment(mipTex.get_weak()), std::format("BloomMipFramebuffer[{}]", i));

		auto downPSO = commonResources->CreateFullscreenPassPSO(fb->GetFramebufferInfo(), m_DownsampleShader.get_weak(),
			std::format("BloomMipDownsamplePSO[{}]", i));

		auto upPSO = device->CreateGraphicsPipelineState(m_BlendPSODesc, fb->GetFramebufferInfo(), std::format("BloomMipUpsamplePSO[{}]", i));

		m_MipChain.emplace_back(std::move(mipTex), std::move(fb), std::move(downPSO), std::move(upPSO));
	}
}