#include "Gfx/GfxPCH.h"
#include "Gfx/MaterialPassRenderer.h"
#include "Gfx/RenderSet.h"
#include "Gfx/Mesh.h"
#include "Gfx/MeshInstance.h"
#include "RHI/Device.h"
#include "RHI/PipelineState.h"
#include "Interop/RenderResources.h"

alm::gfx::MaterialPassRenderer::MaterialPassRenderer() : m_PSOs{}, m_ValidDomains{}, m_Device { nullptr }
{}

void alm::gfx::MaterialPassRenderer::Init(const alm::rhi::GraphicsPipelineStateDesc& baseDesc, const alm::rhi::FramebufferInfo& fbInfo,
	std::string baseDebugName, alm::rhi::Device* device)
{
	m_BasePSODesc = baseDesc;
	m_FBInfo = fbInfo;
	m_BaseDebugName = baseDebugName;
	m_Device = device;
}

void alm::gfx::MaterialPassRenderer::Reset()
{
	for (int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		m_PSOs[domain].PSO_BackCull.reset();
		m_PSOs[domain].PSO_FrontCull.reset();
		m_PSOs[domain].PSO_NoCull.reset();

		m_ValidDomains[domain] = false;
	}

	m_BasePSODesc = {};
	m_FBInfo = {};
	m_BaseDebugName = {};
	m_Device = nullptr;
}

void alm::gfx::MaterialPassRenderer::OnFramebufferChanged(const alm::rhi::FramebufferInfo& fbInfo)
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

void alm::gfx::MaterialPassRenderer::AddDomain(MaterialDomain domain, rhi::ShaderHandle VS, rhi::ShaderHandle PS)
{
	// Init desc shaders
	alm::rhi::GraphicsPipelineStateDesc desc = m_BasePSODesc;
	desc.VS = VS;
	desc.PS = PS;

	// Cull back
	desc.rasterState.cullMode = alm::rhi::CullMode::Back;
	m_PSOs[(int)domain].PSO_BackCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullBack]", m_BaseDebugName, GetMaterialDomainString(domain)));

	// Cull front	
	desc.rasterState.cullMode = alm::rhi::CullMode::Front;
	m_PSOs[(int)domain].PSO_FrontCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullFront]", m_BaseDebugName, GetMaterialDomainString(domain)));

	// Cull none
	desc.rasterState.cullMode = alm::rhi::CullMode::None;
	m_PSOs[(int)domain].PSO_NoCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo, std::format("{}[{}][CullNone]", m_BaseDebugName, GetMaterialDomainString(domain)));

	m_ValidDomains[(int)domain] = true;
}

alm::rhi::IGraphicsPipelineState* alm::gfx::MaterialPassRenderer::GetPSO(MaterialDomain domain, rhi::CullMode cullMode) const
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

void alm::gfx::MaterialPassRenderer::DrawRenderSetInstanced(const alm::gfx::RenderSet& renderSet, alm::rhi::ICommandList* commandList) const
{
	if (renderSet.Elements.empty())
		return;

	interop::MultiInstanceDrawConstants shaderConstants = {};

	int visibleInstanceIndex = 0; // Index to the visible buffer
	for (const auto& domainBase : renderSet.Elements)
	{
		// Ignore not defined domains for this render context
		if (!m_ValidDomains[(int)domainBase.first])
		{
			for (const auto& cullBase : domainBase.second)
			{
				visibleInstanceIndex += cullBase.second.size();
			}
			continue;
		}

		for (const auto& cullBase : domainBase.second)
		{
			if (cullBase.second.empty())
				continue;

			rhi::IGraphicsPipelineState* PSO = GetPSO(domainBase.first, cullBase.first);
			if (!PSO)
			{
				LOG_ERROR("Material domain '{}', Cull mode '{}': No PSO defined in MaterialPassRenderer '{}'",
					GetMaterialDomainString(domainBase.first),
					cullBase.first == rhi::CullMode::Back ? "Back" : cullBase.first == rhi::CullMode::Front ? "Front" : "None",
					m_BaseDebugName);
				continue;
			}

			commandList->SetPipelineState(PSO);
			const auto& drawInfos = cullBase.second;

			uintptr_t batchKey = drawInfos[0].BatchKey;
			int prevIdx = 0;
			for (int i = 1; i < drawInfos.size(); ++i)
			{
				const RenderableDrawInfo& drawInfo = drawInfos[i];
				if (batchKey != drawInfo.BatchKey)
				{
					const RenderableDrawInfo& prevDrawInfo = drawInfos[prevIdx];

					shaderConstants.baseInstanceIdx = visibleInstanceIndex + prevIdx;
					shaderConstants.meshIndex = prevDrawInfo.MeshIndex;
					shaderConstants.materialIndex = prevDrawInfo.MaterialIndex;
					commandList->PushGraphicsConstants(1, shaderConstants);
					commandList->DrawInstanced(prevDrawInfo.IndexCount, i - prevIdx, 0);

					batchKey = drawInfo.BatchKey;
					prevIdx = i;
				}
			}

			const RenderableDrawInfo& drawInfo = drawInfos[prevIdx];
			shaderConstants.baseInstanceIdx = visibleInstanceIndex + prevIdx;
			shaderConstants.meshIndex = drawInfo.MeshIndex;
			shaderConstants.materialIndex = drawInfo.MaterialIndex;
			commandList->PushGraphicsConstants(1, shaderConstants);
			commandList->DrawInstanced(drawInfo.IndexCount, drawInfos.size() - prevIdx, 0);

			visibleInstanceIndex += drawInfos.size();
		}
	}
}