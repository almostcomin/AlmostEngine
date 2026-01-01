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

	rhi::TextureHandle m_SceneColor;
	rhi::GraphicsPipelineStateOwner m_BlitPSO;

	bool Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:
};

} // namespace st::gfx