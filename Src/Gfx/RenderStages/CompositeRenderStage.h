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

	bool Render() override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

	const char* GetDebugName() const override { return "CompositeRenderStage"; }

private:

	rhi::TextureHandle m_SceneColor;
	rhi::GraphicsPipelineStateHandle m_BlitPSO;

	void OnAttached() override;
	void OnDetached() override;

private:
};

} // namespace st::gfx