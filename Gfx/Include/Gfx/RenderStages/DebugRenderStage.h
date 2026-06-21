#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/Framebuffer.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/SceneFlags.h"

namespace alm::gfx
{
	class Scene;
};

namespace alm::gfx
{

class DebugRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(DebugRenderStage)

public:

	DebugRenderStage() = default;

	void ShowRenderBBoxes(SceneContentType boundsType, bool b);
	void ShowHeightmapsBBoxes(bool b) { m_RenderHeightmapBBoxes = b; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	std::pair<rhi::BufferReadOnlyView, size_t> GetAABBOXBuffer(const Scene* scene, SceneContentType boundsType);
	std::pair<rhi::BufferReadOnlyView, size_t> FillBuffer(const std::vector<alm::aabox3f>& aabboxes);

	void CreateBBoxPSO();
	void CreateS2HPSO();

	void RenderBBoxes(alm::rhi::CommandListHandle commandList);
	void RenderS2H(alm::rhi::CommandListHandle commandList);

private:

	RGTextureHandle m_TonemappedTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGTextureHandle m_GBuffer2Texture;
	RGFramebufferHandle m_FB;

	struct
	{
		rhi::GraphicsPipelineStateOwner PSO;
		rhi::ShaderOwner VS;
		rhi::ShaderOwner PS;
	} m_BBoxRenderResources;

	struct
	{
		rhi::ComputePipelineStateOwner PSO;
		rhi::ShaderOwner CS;
	} m_S2HRenderResources;

	std::array<bool, (int)SceneContentType::_Size> m_RenderBBoxes;
	bool m_RenderHeightmapBBoxes = false;
};

} // namespace st::gfx