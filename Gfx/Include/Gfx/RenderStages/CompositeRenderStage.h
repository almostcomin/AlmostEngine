#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/Texture.h"
#include "RHI/PipelineState.h"

namespace st::gfx
{

class CompositeRenderStage : public RenderStage
{
public:

	CompositeRenderStage() = default;

	const char* GetDebugName() const override { return "CompositeRenderStage"; }

private:

	rhi::ShaderOwner m_PS;
	rhi::GraphicsPipelineStateOwner m_PSO;

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;
};

} // namespace st::gfx