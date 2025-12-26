#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"

st::gfx::CommonResources::CommonResources(st::gfx::ShaderFactory* shaderFactory, st::rhi::Device* device) :
	m_ShaderFactory(shaderFactory), m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rhi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rhi::ShaderType::Pixel);

	// Create blit PSO
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
			.VS = m_BlitVS,
			.PS = m_BlitPS,
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState,
			.debugName = "BlitPSO"
		};
	}
}

st::gfx::CommonResources::~CommonResources()
{
	m_Device->ReleaseImmediately(m_BlitVS);
	m_Device->ReleaseImmediately(m_BlitPS);
}

st::rhi::GraphicsPipelineStateHandle st::gfx::CommonResources::CreateBlitPSO(const rhi::FramebufferInfo& fbInfo)
{
	return m_Device->CreateGraphicsPipelineState(m_BlitPSODesc, fbInfo);		
}