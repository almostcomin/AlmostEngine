#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderGraphTypes.h"
#include "Interop/RenderResources.h"

namespace alm::gfx
{

class GPUCullingRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(GPUCullingRenderStage)

public:

	// Enables per-frame GPU stats accumulation (see interop::CullingStats).
	// Ref-counted: GetStats() is only meaningful while there are pending requests.
	void RequestStats();
	void UnrequestStats();
	bool IsStatsRequested() const { return m_StatsRequests != 0; }

	// Returns the last frame's culling stats (1 frame late, same as timer queries).
	// If stats are not active, returns zeros and logs a warning.
	interop::CullingStats GetStats() const;

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

	RGBufferHandle m_IndirectDrawArgsBuffer;
	RGBufferHandle m_SparseArgsBuffer;
	RGBufferHandle m_CountsBuffer;
	RGBufferHandle m_PayloadBuffer;

	RGBufferHandle m_ShadowIndirectDrawArgsBuffer;
	RGBufferHandle m_SparseShadowArgsBuffer;
	RGBufferHandle m_ShadowCountsBuffer;
	RGBufferHandle m_ShadowPayloadBuffer;

	alm::rhi::BufferOwner m_StatsBuffer;
	alm::rhi::BufferOwner m_StatsBufferRB;
	interop::CullingStats* m_StatsBufferRBPtr = nullptr;   // Cached mapped pointer of the readback buffer

	alm::rhi::ShaderOwner m_PrepareCS;
	alm::rhi::ShaderOwner m_CullingCS;
	alm::rhi::ShaderOwner m_CullingStatsCS;
	alm::rhi::ShaderOwner m_CompactCS;

	alm::rhi::ComputePipelineStateOwner m_PreparePSO;
	alm::rhi::ComputePipelineStateOwner m_CullingPSO;
	alm::rhi::ComputePipelineStateOwner m_CullingStatsPSO;
	alm::rhi::ComputePipelineStateOwner m_CompactPSO;

	uint32_t m_StatsRequests = 0;
};

} // namespace alm::gfx