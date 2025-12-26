#pragma once

#include "Gfx/RenderStage.h"
#include "RenderAPI/PipelineState.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/FrameBuffer.h"

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

	bool Render() override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

	const char* GetDebugName() const override { return "DebugRenderStage"; }

private:

	void OnAttached() override;
	void OnDetached() override;

	std::pair<rapi::DescriptorIndex, size_t> GetAABBOXBuffer(const Scene* scene, rapi::CommandListHandle commandList);

	rapi::GraphicsPipelineStateHandle m_PSO;
	rapi::ShaderHandle m_VS;
	rapi::ShaderHandle m_PS;
	rapi::BufferHandle m_AABBOXBuffer;
	rapi::FramebufferHandle m_FB;
};

} // namespace st::gfx