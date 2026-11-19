#pragma once

#include "Gfx/MaterialDomain.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/RenderSet.h"
#include "RHI/PipelineState.h"
#include "RHI/Framebuffer.h"

namespace alm::rhi
{
	struct GraphicsPipelineStateDesc;
	struct FramebufferInfo;
	class Device;
}

namespace alm::gfx
{
	class IDrawStrategy;
}

namespace alm::gfx
{

class MeshPassProcessor
{
public:

	struct IndirectDrawParams
	{
		rhi::IBuffer* ArgsBuffer = nullptr;
		rhi::IBuffer* CountsBuffer = nullptr;
		const GpuSceneBuffers::BucketInfoArray* Buckets = nullptr;
		const std::span<const GpuSceneBuffers::TransientRecord> Transients;
	};

	struct RenderStats
	{
		uint32_t VisibleInstances = 0;
		uint32_t VisiblePrimitives = 0;
	};

public:

	MeshPassProcessor();
	
	void Init(const alm::rhi::GraphicsPipelineStateDesc& baseDesc, const alm::rhi::FramebufferInfo& fbInfo,
		std::string baseDebugName, alm::rhi::Device* device);
	void Reset();

	void OnFramebufferChanged(const alm::rhi::FramebufferInfo& fbInfo);

	void AddDomain(MaterialDomain::Type domain, rhi::ShaderHandle VS, rhi::ShaderHandle PS);

	// Returns the workload submitted: instances drawn and primitives rasterized.
	// Fallback to CanonicalDrawStrategy
	RenderStats DrawRenderSetInstanced(const RenderSet& renderSet, alm::rhi::ICommandList* commandList) const;

	RenderStats DrawRenderSetInstanced(const RenderSet& renderSet, alm::gfx::IDrawStrategy& drawStrategy,
		alm::rhi::ICommandList* commandList) const;

	void DrawIndirect(const IndirectDrawParams& params, alm::rhi::ICommandList* commandList) const;

	// Sums the per-bucket culling stats over this renderer's valid domains:
	// the workload this pass actually submits (Visible = what its ExecuteIndirects draw).
	// shadowFrustum selects between the camera and the shadow frustum's stats.
	RenderStats ComputeRenderedStats(const interop::CullingStats& stats, bool shadowFrustum) const;

	const std::string& GetDebugName() const { return m_BaseDebugName; }

private:

	rhi::IGraphicsPipelineState* GetPSO(MaterialDomain::Type domain, rhi::CullMode cullMode) const;

private:

	struct CullPSOs
	{
		rhi::GraphicsPipelineStateOwner PSO_BackCull;
		rhi::GraphicsPipelineStateOwner PSO_FrontCull;
		rhi::GraphicsPipelineStateOwner PSO_NoCull;
	};

	struct ShaderEntryType
	{
		rhi::ShaderHandle VS;
		rhi::ShaderHandle PS;
	};

	alm::rhi::GraphicsPipelineStateDesc m_BasePSODesc;
	std::array<CullPSOs, (int)MaterialDomain::_Size> m_PSOs;
	std::array<bool, (int)MaterialDomain::_Size> m_ValidDomains;
	alm::rhi::FramebufferInfo m_FBInfo;

	alm::rhi::CommandSignatureOwner m_CommandSignature;

	std::string m_BaseDebugName;
	rhi::Device* m_Device;
};

} //namespace alm::gfx