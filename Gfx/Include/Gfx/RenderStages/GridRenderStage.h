#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "Interop/RenderResources.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/Framebuffer.h"

#include <vector>

namespace alm::gfx
{

class GridRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(GridRenderStage)

public:

	GridRenderStage() = default;

	void SetVisible(bool b) { m_Visible = b; }
	bool IsVisible() const { return m_Visible; }

	// Changing the extent regenerates the per-frame vertex buffers (rare, UI-driven)
	void SetExtent(float extent) { m_Extent = extent; }
	float GetExtent() const { return m_Extent; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	void CreatePSO();
	// Worst-case buffer size (bytes) needed to hold the grid geometry for the current extent
	size_t ComputeRequiredBufferSize() const;
	// Recreates the per-frame vertex buffers when their size no longer matches the required size
	void EnsureFrameBuffers();
	// Writes grid vertices to dst. Returns the vertex count.
	size_t BuildGridGeometry(interop::GridVertex* dst, size_t maxVertexCount);

private:

	RGTextureHandle m_TonemappedTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGFramebufferHandle m_FB;

	struct
	{
		rhi::GraphicsPipelineStateOwner PSO;
		rhi::ShaderOwner VS;
		rhi::ShaderOwner PS;
	} m_RenderResources;

	// Persistent upload buffers, one per frame in flight, mapped and refilled every frame.
	// Sized to fit the worst-case geometry implied by m_Extent and recreated when it changes.
	std::vector<rhi::BufferOwner> m_FrameVertexBuffers;
	size_t m_BufferSizeBytes = 0;

	bool m_Visible = true;

	float m_Extent = 100.f;
	float m_MinorStep = 1.f;
	int m_MajorStep = 10;
	float m_CutDistance = 90.f;
};

} // namespace alm::gfx
