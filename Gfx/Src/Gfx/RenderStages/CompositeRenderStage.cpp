#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

bool st::gfx::CompositeRenderStage::Render()
{
	auto commandList = m_RenderView->GetCommandList();

	auto fb = m_RenderView->GetFramebuffer();
	commandList->BeginRenderPass(
		fb.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_BlitPSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)fb->GetFramebufferInfo().width, (float)fb->GetFramebufferInfo().height }));

	interop::BlitConstants shaderConstants;
	shaderConstants.textureDI = m_SceneColor->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);

	commandList->PushConstants(shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();

	return true;
}

void st::gfx::CompositeRenderStage::OnAttached()
{
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor", 
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_SceneColor = m_RenderView->GetTexture("SceneColor");

	m_BlitPSO = m_RenderView->GetDeviceManager()->GetCommonResources()->CreateBlitPSO(
		m_RenderView->GetFramebuffer()->GetFramebufferInfo());
}

void st::gfx::CompositeRenderStage::OnDetached()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_BlitPSO);
	m_SceneColor = nullptr;
}

void st::gfx::CompositeRenderStage::OnBackbufferResize()
{
	m_SceneColor = m_RenderView->GetTexture("SceneColor");

	m_BlitPSO = m_RenderView->GetDeviceManager()->GetCommonResources()->CreateBlitPSO(
		m_RenderView->GetFramebuffer()->GetFramebufferInfo());
}