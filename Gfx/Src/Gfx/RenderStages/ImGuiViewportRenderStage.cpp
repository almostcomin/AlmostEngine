#include "Gfx/RenderStages/ImGuiViewportRenderStage.h"
#include "Gfx/RenderView.h"

st::gfx::ImGuiViewportRenderStage::ImGuiViewportRenderStage(ImGuiRenderStage* mainViewportRS, ImGuiViewport* imGuiViewport) :
	m_MainRS{ mainViewportRS }, m_ImGuiViewport{ imGuiViewport }
{}

void st::gfx::ImGuiViewportRenderStage::Render()
{
	auto commandList = m_RenderView->GetCommandList();
	auto fb = m_RenderView->GetFramebuffer();

	commandList->BeginRenderPass(
		fb.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		{},
		{},
		rhi::RenderPassFlags::None);

	m_MainRS->RenderDrawData(m_ImGuiViewport->DrawData, m_GeometryBuffers, m_RenderView->GetCommandList().get());

	commandList->EndRenderPass();
}

void st::gfx::ImGuiViewportRenderStage::OnAttached()
{
}

void st::gfx::ImGuiViewportRenderStage::OnDetached()
{
	m_GeometryBuffers = {};
}
