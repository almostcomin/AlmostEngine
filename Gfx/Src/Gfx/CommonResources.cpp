#include "Gfx/GfxPCH.h"
#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"

alm::gfx::CommonResources::CommonResources(alm::gfx::ShaderFactory* shaderFactory, alm::rhi::Device* device) :
	m_ShaderFactory(shaderFactory), m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rhi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rhi::ShaderType::Pixel);
	m_BlitCS = m_ShaderFactory->LoadShader("Blit_cs", rhi::ShaderType::Compute);
	m_ClearBufferCS = m_ShaderFactory->LoadShader("ClearBuffer_cs", rhi::ShaderType::Compute);
	m_ClearTextureCS = m_ShaderFactory->LoadShader("ClearTexture_cs", rhi::ShaderType::Compute);

	// Create blit PSO desc
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
		{
			.blendEnable = false,
		};

		rhi::RasterizerState rasterState =
		{
			.fillMode = rhi::FillMode::Solid,
			.cullMode = rhi::CullMode::None
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilEnable = false
		};

		m_BlitGraphicsPSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_BlitVS.get_weak(),
			.PS = m_BlitPS.get_weak(),
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState,
		};
	}

	// Compute blit
	{
		m_BlitComputePSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BlitCS.get_weak() }, "BlitComputePSO");
	}

	// Compute ClearBuffer PSO
	{
		m_ClearBufferPSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearBufferCS.get_weak() }, "ClearBufferPSO");
	}

	// Compute ClearTexture PSO
	{
		m_ClearTexturePSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearTextureCS.get_weak() }, "ClearTexturePSO");
	}
}

alm::gfx::CommonResources::~CommonResources()
{
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo)
{
	return m_Device->CreateGraphicsPipelineState(m_BlitGraphicsPSODesc, fbInfo, "BlitGraphicsPSO");
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo,
	const rhi::ShaderHandle& PS, const std::string debugName)
{
	auto desc = m_BlitGraphicsPSODesc;
	desc.PS = PS;

	return m_Device->CreateGraphicsPipelineState(desc, fbInfo, debugName);
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo,
	const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS, const std::string debugName)
{
	auto desc = m_BlitGraphicsPSODesc;
	desc.VS = VS;
	desc.PS = PS;

	return m_Device->CreateGraphicsPipelineState(desc, fbInfo, debugName);
}
