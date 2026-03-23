#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/FrameBuffer.h"
#include "Gfx/SceneBounds.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{
	class Scene;
};

namespace alm::gfx
{

class DebugRenderStage : public RenderStage
{
public:

	DebugRenderStage() = default;

	void ShowRenderBBoxes(BoundsType boundsType, bool b) { m_RenderBBoxes[(int)boundsType] = b; }

	const char* GetDebugName() const override { return "DebugRenderStage"; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	std::pair<rhi::BufferReadOnlyView, size_t> GetAABBOXBuffer(const Scene* scene, BoundsType boundsType, rhi::CommandListHandle commandList);

private:

	RGTextureHandle m_TonemappedTexture;
	RGTextureHandle m_SceneDepthTexture;

	rhi::GraphicsPipelineStateOwner m_PSO;
	rhi::ShaderOwner m_VS;
	rhi::ShaderOwner m_PS;
	rhi::BufferOwner m_AABBOXBuffer;
	rhi::FramebufferOwner m_FB;

	std::array<bool, (int)BoundsType::_Size> m_RenderBBoxes;
};

} // namespace st::gfx