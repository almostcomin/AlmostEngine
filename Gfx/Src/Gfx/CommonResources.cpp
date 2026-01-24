#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"

st::gfx::CommonResources::CommonResources(st::gfx::ShaderFactory* shaderFactory, st::rhi::Device* device) :
	m_ShaderFactory(shaderFactory), m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rhi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rhi::ShaderType::Pixel);

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

		m_BlitPSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_BlitVS.get_weak(),
			.PS = m_BlitPS.get_weak(),
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState,
		};
	}
}

st::gfx::CommonResources::~CommonResources()
{
}

st::rhi::GraphicsPipelineStateOwner st::gfx::CommonResources::CreateBlitPSO(const rhi::FramebufferInfo& fbInfo)
{
	return m_Device->CreateGraphicsPipelineState(m_BlitPSODesc, fbInfo, "BlitPSO");
}

st::rhi::GraphicsPipelineStateOwner st::gfx::CommonResources::CreateBlitPSO(const rhi::FramebufferInfo& fbInfo,
	const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS, const std::string debugName)
{
	auto desc = m_BlitPSODesc;
	desc.VS = VS;
	desc.PS = PS;

	return m_Device->CreateGraphicsPipelineState(desc, fbInfo, debugName);
}