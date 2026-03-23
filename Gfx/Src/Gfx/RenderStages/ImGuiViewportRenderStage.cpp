#include "Gfx/RenderStages/ImGuiViewportRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraphBuilder.h"

alm::gfx::ImGuiViewportRenderStage::ImGuiViewportRenderStage(ImGuiRenderStage* mainViewportRS, ImGuiViewport* imGuiViewport) :
	m_MainRS{ mainViewportRS }, m_ImGuiViewport{ imGuiViewport }
{}

void alm::gfx::ImGuiViewportRenderStage::Setup(RenderGraphBuilder& builder)
{
	builder.AddRenderTargetWriteDependency();
}

void alm::gfx::ImGuiViewportRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto fb = m_RenderGraph->GetFramebuffer();

	commandList->BeginRenderPass(
		fb.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		{},
		{},
		rhi::RenderPassFlags::None);

	m_MainRS->RenderDrawData(m_ImGuiViewport->DrawData, m_GeometryBuffers, commandList.get());

	commandList->EndRenderPass();
}

void alm::gfx::ImGuiViewportRenderStage::OnAttached()
{
}

void alm::gfx::ImGuiViewportRenderStage::OnDetached()
{
	m_GeometryBuffers = {};
}
