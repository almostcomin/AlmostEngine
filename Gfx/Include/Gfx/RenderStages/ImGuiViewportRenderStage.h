#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{
	class ImGuiRenderStage;
}

namespace alm::gfx
{

class ImGuiViewportRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(ImGuiViewportRenderStage);

public:

	ImGuiViewportRenderStage();
	
	void Init(ImGuiRenderStage* mainViewportRS, ImGuiViewport* imGuiViewport);

	const ImGuiViewport* GetImGuiViewport() const { return m_ImGuiViewport; }

private:

	ImGuiRenderStage* m_MainRS;
	ImGuiViewport* m_ImGuiViewport;

	alm::gfx::ImGuiRenderStage::GeometryBuffers m_GeometryBuffers;

	std::string m_Title;

protected:

	void Setup(RenderGraphBuilder& builder) override;

	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
};

} // namespace st::gfx