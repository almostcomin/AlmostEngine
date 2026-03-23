#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::gfx
{

	class LinearizeDepthRenderStage : public RenderStage
	{
	public:

		LinearizeDepthRenderStage() = default;

		virtual const char* GetDebugName() const { return "LinearizeDepthRenderStage"; }

	private:

		void Setup(RenderGraphBuilder& builder);
		void Render(alm::rhi::CommandListHandle commandList) override;
		void OnAttached() override;
		void OnDetached() override;

	private:

		RGTextureHandle m_LinearDepthTexture;
		RGTextureHandle m_SceneDepthTexture;

		rhi::ShaderOwner m_LinearizeDepthCS;
		rhi::ComputePipelineStateOwner m_LinearizeDepthPSO;
	};

} // namespace st::gfx