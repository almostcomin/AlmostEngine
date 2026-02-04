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

	float GetPaperWhiteNits() const { return m_PaperWhiteNits; }
	void SetPaperWhiteNits(float v) { m_PaperWhiteNits = v; }

	const char* GetDebugName() const override { return "CompositeRenderStage"; }

private:

	float m_PaperWhiteNits = 203.f;

	rhi::ShaderOwner m_PS;
	rhi::GraphicsPipelineStateOwner m_PSO;

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;
};

} // namespace st::gfx