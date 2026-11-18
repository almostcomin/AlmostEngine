#include "Gfx/GfxPCH.h"
#include "Gfx/MaterialPassRenderer.h"
#include "Gfx/RenderSet.h"
#include "Gfx/Mesh.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/IDrawStrategy.h"
#include "Gfx/CanonicalDrawStrategy.h"
#include "RHI/Device.h"
#include "RHI/PipelineState.h"
#include "RHI/CommandSignature.h"
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

	m_CommandSignature = device->CreateCommandSignature(
		rhi::CommandSignatureDesc{ .commandType = rhi::CommandSignatureDesc::CommandType::Draw },
		baseDebugName);
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

	m_CommandSignature = {};
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

	// Cull none
	desc.rasterState.cullMode = alm::rhi::CullMode::None;
	m_PSOs[(int)domain].PSO_NoCull =
		m_Device->CreateGraphicsPipelineState(desc, m_FBInfo,
			std::format("{}[{}][CullNone]", m_BaseDebugName, GetMaterialDomainString(domain)));

	if (!SoftwareCull(domain))
	{
		// Cull back
		desc.rasterState.cullMode = alm::rhi::CullMode::Back;
		m_PSOs[(int)domain].PSO_BackCull =
			m_Device->CreateGraphicsPipelineState(desc, m_FBInfo,
				std::format("{}[{}][CullBack]", m_BaseDebugName, GetMaterialDomainString(domain)));

		// Cull front	
		desc.rasterState.cullMode = alm::rhi::CullMode::Front;
		m_PSOs[(int)domain].PSO_FrontCull =
			m_Device->CreateGraphicsPipelineState(desc, m_FBInfo,
				std::format("{}[{}][CullFront]", m_BaseDebugName, GetMaterialDomainString(domain)));
	}

	m_ValidDomains[(int)domain] = true;
}

alm::gfx::MaterialPassRenderer::RenderStats alm::gfx::MaterialPassRenderer::DrawRenderSetInstanced(
	const RenderSet& renderSet, alm::rhi::ICommandList* commandList) const
{
	CanonicalDrawStrategy drawStrategy;
	return DrawRenderSetInstanced(renderSet, drawStrategy, commandList);
}

alm::gfx::MaterialPassRenderer::RenderStats alm::gfx::MaterialPassRenderer::DrawRenderSetInstanced(
	const RenderSet& renderSet, alm::gfx::IDrawStrategy& drawStrategy, alm::rhi::ICommandList* commandList) const
{
	RenderStats out{};

	drawStrategy.Prepare(renderSet);

	rhi::IGraphicsPipelineState* prevPSO = nullptr;
	interop::MaterialPassRendererConstants shaderConstants = {};
	while (true)
	{
		IDrawStrategy::DrawBucket drawBucket = drawStrategy.GetNextBucket();
		if (drawBucket.DrawInfos.empty())
			break;

		const RenderableDrawInfo& firstDrawInfo = drawBucket.DrawInfos.front();
		const MaterialDomain domain = firstDrawInfo.MaterialDomain;
		const rhi::CullMode cullMode = SoftwareCull(domain) ? rhi::CullMode::None : firstDrawInfo.CullMode;

		if (!m_ValidDomains[(int)domain])
			continue;   // Domain not registered: Skip

		rhi::IGraphicsPipelineState* PSO = GetPSO(domain, cullMode);
		if (!PSO)
		{
			LOG_ERROR("Material domain '{}', Cull mode '{}': No PSO defined in MaterialPassRenderer '{}'",
				GetMaterialDomainString(domain), rhi::GetCullModeString(cullMode),
				m_BaseDebugName);
			continue;
		}
		if (PSO != prevPSO)
		{
			commandList->SetPipelineState(PSO);
			prevPSO = PSO;
		}

		shaderConstants.baseInstanceIdx = drawBucket.FirstSlot;
		shaderConstants.rawInstanceBaseIdx = firstDrawInfo.InstanceIdx;
		shaderConstants.meshIndex = firstDrawInfo.MeshIndex;
		shaderConstants.materialIndex = firstDrawInfo.MaterialIndex;
		shaderConstants.extraDataBaseIdx = firstDrawInfo.TransientBaseIndex;
		commandList->PushGraphicsConstants(1, shaderConstants);
		commandList->DrawInstanced(firstDrawInfo.IndexCount, drawBucket.DrawInfos.size(), 0);

		out.VisibleInstances += drawBucket.DrawInfos.size();
		out.VisiblePrimitives += GetPrimitiveCount(
			firstDrawInfo.IndexCount, m_BasePSODesc.primTopo) * drawBucket.DrawInfos.size();
	}

	return out;
}

void alm::gfx::MaterialPassRenderer::DrawIndirect(const IndirectDrawParams& params, alm::rhi::ICommandList* commandList) const
{
	assert(params.ArgsBuffer && params.Buckets);
	
	rhi::IGraphicsPipelineState* prevPSO = nullptr;
	for (int domainIdx = 0; domainIdx < (int)MaterialDomain::_Size; ++domainIdx)
	{
		if (!m_ValidDomains[domainIdx])
			continue;

		const MaterialDomain domain = (MaterialDomain)domainIdx;
		for (int cullModeIdx = 0; cullModeIdx < (int)rhi::CullMode::_Size; ++cullModeIdx)
		{
			const auto& bucket = (*params.Buckets)[domainIdx][cullModeIdx];
			if (bucket.BatchCount == 0)
				continue;

			const rhi::CullMode cullMode = SoftwareCull(domain) ? rhi::CullMode::None : (rhi::CullMode)cullModeIdx;
			rhi::IGraphicsPipelineState* PSO = GetPSO(domain, cullMode);
			if (!PSO)
			{
				LOG_ERROR("Material domain '{}', Cull mode '{}': No PSO defined in MaterialPassRenderer '{}'",
					GetMaterialDomainString(domain), rhi::GetCullModeString(cullMode),
					m_BaseDebugName);
				continue;
			}
			if (PSO != prevPSO)
			{
				commandList->SetPipelineState(PSO);
				prevPSO = PSO;
			}

			commandList->BeginMarker(std::format("{}:Static - {}:{}", m_BaseDebugName,
				GetMaterialDomainString(domain), rhi::GetCullModeString(cullMode)).c_str());

			commandList->ExecuteIndirect(
				m_CommandSignature.get(),
				params.ArgsBuffer,
				bucket.FirstBatchIndex * sizeof(interop::IndirectDrawCommand),
				params.CountsBuffer,
				bucket.FirstBatchIndex * sizeof(uint32_t),
				bucket.BatchCount);

			commandList->EndMarker();
		}
	}

	for (const auto& t : params.Transients)
	{
		if (!m_ValidDomains[(int)t.Domain])
			continue;

		const rhi::CullMode cullMode = SoftwareCull(t.Domain) ? rhi::CullMode::None : t.Cull;
		rhi::IGraphicsPipelineState* PSO = GetPSO(t.Domain, cullMode);
		if (!PSO)
		{
			LOG_ERROR("Material domain '{}', Cull mode '{}': No PSO defined in MaterialPassRenderer '{}'",
				GetMaterialDomainString(t.Domain), rhi::GetCullModeString(cullMode),
				m_BaseDebugName);
			continue;
		}
		if (PSO != prevPSO)
		{
			commandList->SetPipelineState(PSO);
			prevPSO = PSO;
		}

		commandList->BeginMarker(std::format("{}:{} - {}:{}", m_BaseDebugName, t.DebugName.empty() ? "Transient" : t.DebugName,
			GetMaterialDomainString(t.Domain), rhi::GetCullModeString(cullMode)).c_str());

		commandList->ExecuteIndirect(
			m_CommandSignature.get(),
			params.ArgsBuffer,
			t.FirstBatchIndex * sizeof(interop::IndirectDrawCommand),
			params.CountsBuffer,
			t.FirstBatchIndex * sizeof(uint32_t),
			t.BatchCount);

		commandList->EndMarker();
	}
}

alm::gfx::MaterialPassRenderer::RenderStats alm::gfx::MaterialPassRenderer::ComputeRenderedStats(
	const interop::CullingStats& stats, bool shadowFrustum) const
{
	RenderStats out{};
	for (int domainIdx = 0; domainIdx < (int)MaterialDomain::_Size; ++domainIdx)
	{
		if (!m_ValidDomains[domainIdx])
			continue;

		for (int cullIdx = 0; cullIdx < (int)rhi::CullMode::_Size; ++cullIdx)
		{
			const uint32_t bucketIdx = (uint32_t)(domainIdx * interop::CullModeCount + cullIdx);
			const interop::CullingBucketStats& bucketStats = stats.Buckets[bucketIdx];
			if (shadowFrustum)
			{
				out.VisibleInstances += bucketStats.ShadowVisibleInstances;
				out.VisiblePrimitives += bucketStats.ShadowVisiblePrimitives;
			}
			else
			{
				out.VisibleInstances += bucketStats.CameraVisibleInstances;
				out.VisiblePrimitives += bucketStats.CameraVisiblePrimitives;
			}
		}
	}
	return out;
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