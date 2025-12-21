#pragma once

#include "Gfx/RenderStage.h"
#include "RenderAPI/Texture.h"
#include "RenderAPI/PipelineState.h"

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

	rapi::TextureHandle m_SceneColor;
	rapi::GraphicsPipelineStateHandle m_BlitPSO;

	void OnAttached() override;
	void OnDetached() override;

private:
};

} // namespace st::gfx