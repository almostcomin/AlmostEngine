#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"

namespace st::gfx
{
	class ImGuiRenderStage;
}

namespace st::gfx
{

class ImGuiViewportRenderStage : public RenderStage
{
public:

	ImGuiViewportRenderStage(ImGuiRenderStage* mainViewportRS, ImGuiViewport* imGuiViewport);

	const ImGuiViewport* GetImGuiViewport() const { return m_ImGuiViewport; }

	const char* GetDebugName() const override { return "ImGuiViewportRenderStage"; }

private:

	ImGuiRenderStage* m_MainRS;
	ImGuiViewport* m_ImGuiViewport;

	st::gfx::ImGuiRenderStage::GeometryBuffers m_GeometryBuffers;

	std::string m_Title;

protected:

	void Setup(RenderGraphBuilder& builder) override;

	void Render(st::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
};

} // namespace st::gfx