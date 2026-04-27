#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/FrameBuffer.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/SceneContentFlags.h"

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

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	std::pair<rhi::BufferReadOnlyView, size_t> GetAABBOXBuffer(const Scene* scene, SceneContentType boundsType, rhi::CommandListHandle commandList);

private:

	RGTextureHandle m_TonemappedTexture;
	RGTextureHandle m_SceneDepthTexture;

	rhi::GraphicsPipelineStateOwner m_PSO;
	rhi::ShaderOwner m_VS;
	rhi::ShaderOwner m_PS;
	rhi::BufferOwner m_AABBOXBuffer;
	rhi::FramebufferOwner m_FB;

	std::array<bool, (int)SceneContentType::_Size> m_RenderBBoxes;
};

} // namespace st::gfx