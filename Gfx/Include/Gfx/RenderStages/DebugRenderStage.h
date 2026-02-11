#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/FrameBuffer.h"

namespace st::gfx
{
	class Scene;
};

namespace st::gfx
{

class DebugRenderStage : public RenderStage
{
public:

	DebugRenderStage() = default;

	void SetRenderBBoxes(bool b) { m_RenderBBoxes = b; }

	const char* GetDebugName() const override { return "DebugRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	std::pair<rhi::BufferReadOnlyView, size_t> GetAABBOXBuffer(const Scene* scene, rhi::CommandListHandle commandList);

	rhi::GraphicsPipelineStateOwner m_PSO;
	rhi::ShaderOwner m_VS;
	rhi::ShaderOwner m_PS;
	rhi::BufferOwner m_AABBOXBuffer;
	rhi::FramebufferOwner m_FB;

	bool m_RenderBBoxes = false;
};

} // namespace st::gfx