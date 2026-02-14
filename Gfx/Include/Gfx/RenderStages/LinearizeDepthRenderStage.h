#pragma once

#include "Gfx/RenderStage.h"

namespace st::gfx
{

	class LinearizeDepthRenderStage : public RenderStage
	{
	public:

		LinearizeDepthRenderStage() = default;

		virtual const char* GetDebugName() const { return "LinearizeDepthRenderStage"; }

	private:

		void Render() override;
		void OnAttached() override;
		void OnDetached() override;

	private:

		rhi::ShaderOwner m_LinearizeDepthCS;
		rhi::ComputePipelineStateOwner m_LinearizeDepthPSO;
	};

} // namespace st::gfx