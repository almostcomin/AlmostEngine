#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

class ToneMappingRenderStage : public RenderStage
{
public:

	ToneMappingRenderStage() = default;

	const char* GetDebugName() const override { return "ToneMappingRenderStage"; }

private:

	void Render() override;
	void OnAttached() override;
	void OnDetached() override;

private:

	rhi::ShaderOwner m_CS;
	rhi::ComputePipelineStateOwner m_PSO;
};

} // namespace st::gfx