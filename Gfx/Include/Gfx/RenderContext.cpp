#include "Gfx/RenderContext.h"
#include "RHI/Device.h"
#include "RHI/PipelineState.h"

st::gfx::RenderContext::RenderContext() : m_Device{ nullptr } 
{}

void st::gfx::RenderContext::Init(const st::rhi::GraphicsPipelineStateDesc& baseDesc, const st::rhi::FramebufferInfo& fbInfo, 
	std::string baseDebugName, st::rhi::Device* device)
{
	m_BasePSODesc = baseDesc;
	m_FBInfo = fbInfo;
	m_BaseDebugName = baseDebugName;
	m_Device = device;
}

void st::gfx::RenderContext::Reset()
{
	for (int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		m_PSOs[domain].PSO_BackCull.reset();
		m_PSOs[domain].PSO_FrontCull.reset();
		m_PSOs[domain].PSO_NoCull.reset();
	}

	m_BasePSODesc = {};
	m_FBInfo = {};
	m_BaseDebugName = {};
	m_Device = nullptr;
}

void st::gfx::RenderContext::OnFramebufferChanged(const st::rhi::FramebufferInfo& fbInfo)
{
	m_FBInfo = fbInfo;

	for (int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		if (m_PSOs[domain].PSO_BackCull)
		{
			const auto& desc = m_PSOs[domain].PSO_BackCull->GetDesc();
			const auto& debugName = m_PSOs[domain].PSO_BackCull->GetDebugName();
			m_PSOs[domain].PSO_BackCull =
				m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, debugName);
		}
		if (m_PSOs[domain].PSO_FrontCull)
		{
			const auto& desc = m_PSOs[domain].PSO_FrontCull->GetDesc();
			const auto& debugName = m_PSOs[domain].PSO_FrontCull->GetDebugName();
			m_PSOs[domain].PSO_FrontCull =
				m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, debugName);
		}
		if (m_PSOs[domain].PSO_NoCull)
		{
			const auto& desc = m_PSOs[domain].PSO_NoCull->GetDesc();
			const auto& debugName = m_PSOs[domain].PSO_NoCull->GetDebugName();
			m_PSOs[domain].PSO_NoCull =
				m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, debugName);
		}
	}
}

void st::gfx::RenderContext::AddDomain(MaterialDomain domain, rhi::ShaderHandle VS, rhi::ShaderHandle PS)
{
	// Init desc shaders
	st::rhi::GraphicsPipelineStateDesc desc = m_BasePSODesc;
	desc.VS = VS;
	desc.PS = PS;

	// Cull back
	desc.rasterState.cullMode = st::rhi::CullMode::Back;
	m_PSOs[(int)domain].PSO_BackCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullBack]", m_BaseDebugName, GetMaterialDomainString(domain)));

	// Cull front	
	desc.rasterState.cullMode = st::rhi::CullMode::Front;
	m_PSOs[(int)domain].PSO_FrontCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullFront]", m_BaseDebugName, GetMaterialDomainString(domain)));

	// Cull none
	desc.rasterState.cullMode = st::rhi::CullMode::None;
	m_PSOs[(int)domain].PSO_NoCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullNone]", m_BaseDebugName, GetMaterialDomainString(domain)));
}

st::rhi::IGraphicsPipelineState* st::gfx::RenderContext::Get(MaterialDomain domain, rhi::CullMode cullMode) const
{
	switch (cullMode)
	{
	case rhi::CullMode::Back:
		return m_PSOs[(int)domain].PSO_BackCull.get();
	case rhi::CullMode::Front:
		return m_PSOs[(int)domain].PSO_FrontCull.get();
	case rhi::CullMode::None:
		return m_PSOs[(int)domain].PSO_NoCull.get();
	default:
		assert(0);
	}

	return nullptr;
}