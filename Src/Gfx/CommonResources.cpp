#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "RenderAPI/Device.h"

st::gfx::CommonResources::CommonResources(st::gfx::ShaderFactory* shaderFactory, st::rapi::Device* device) :
	m_ShaderFactory(shaderFactory), m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rapi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rapi::ShaderType::Pixel);

	// Create blit PSO
	{
		rapi::BlendState blendState;
		blendState.renderTarget[0] = rapi::BlendState::RenderTargetBlendState
		{
			.blendEnable = false,
		};

		rapi::RasterizerState rasterState =
		{
			.fillMode = rapi::FillMode::Solid,
			.cullMode = rapi::CullMode::None
		};

		rapi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilEnable = false
		};

		m_BlitPSODesc = rapi::GraphicsPipelineStateDesc
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

st::rapi::GraphicsPipelineStateHandle st::gfx::CommonResources::CreateBlitPSO(const rapi::FramebufferInfo& fbInfo)
{
	return m_Device->CreateGraphicsPipelineState(m_BlitPSODesc, fbInfo);		
}