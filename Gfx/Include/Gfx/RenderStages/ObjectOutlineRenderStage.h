#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include "RHI/PipelineState.h"

namespace alm::gfx
{
	class MeshInstance;
}

namespace alm::gfx
{

class ObjectOutlineRenderStage : public alm::gfx::RenderStage
{
	REGISTER_RENDER_STAGE(ObjectOutlineRenderStage)

public:

	ObjectOutlineRenderStage() = default;

	void SetSelectedObject(const alm::weak<alm::gfx::MeshInstance>& obj) { m_SelectedObject = obj; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	RGTextureHandle m_ToneMappedTexture;
	RGTextureHandle m_SceneDepthTexture;
	RGFramebufferHandle m_FB;

	alm::rhi::ShaderOwner m_VS;
	alm::rhi::ShaderOwner m_PS;
	alm::rhi::GraphicsPipelineStateOwner m_MaskPSO;
	alm::rhi::GraphicsPipelineStateOwner m_HullPSO;

	alm::rhi::GraphicsPipelineStateDesc m_MaskPSODesc;
	alm::rhi::GraphicsPipelineStateDesc m_HullPSODesc;

	alm::weak<alm::gfx::MeshInstance> m_SelectedObject;
};

} // namespace alm::gfx