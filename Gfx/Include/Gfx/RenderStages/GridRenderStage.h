#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/CommandList.h"
#include "RHI/Framebuffer.h"

namespace alm::gfx
{

class GridRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(GridRenderStage)

public:

	GridRenderStage() = default;

	void SetVisible(bool b) { m_Visible = b; }
	bool IsVisible() const { return m_Visible; }

	void SetExtent(float extent) { m_Extent = extent; }
	float GetExtent() const { return m_Extent; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	void CreatePSO();
	std::pair<rhi::BufferReadOnlyView, size_t> BuildGridGeometry();

private:

	RGTextureHandle m_TonemappedTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGFramebufferHandle m_FB;

	struct
	{
		rhi::GraphicsPipelineStateOwner PSO;
		rhi::ShaderOwner VS;
		rhi::ShaderOwner PS;
	} m_RenderResources;

	bool m_Visible = true;

	float m_Extent = 100.f;
	float m_MinorStep = 1.f;
	int m_MajorStep = 10;
	float m_FadeStart = 40.f;
	float m_FadeEnd = 90.f;
};

} // namespace alm::gfx
