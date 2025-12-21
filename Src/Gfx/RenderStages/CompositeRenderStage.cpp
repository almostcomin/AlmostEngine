#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/CommonResources.h"
#include "Interop/RenderResources.h"

bool st::gfx::CompositeRenderStage::Render()
{
	auto commandList = m_RenderView->GetCommandList();

	auto fb = m_RenderView->GetFramebuffer();
	commandList->BeginRenderPass(
		fb.get(),
		{ rapi::RenderPassOp{rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::Black()} },
		{},
		rapi::RenderPassFlags::None);

	commandList->SetPipelineState(m_BlitPSO.get());

	commandList->SetViewport(rapi::ViewportState().AddViewportAndScissorRect({
		(float)fb->GetFramebufferInfo().width, (float)fb->GetFramebufferInfo().height }));

	interop::BlitConstants shaderConstants;
	shaderConstants.textureDI = m_SceneColor->GetShaderViewIndex(rapi::TextureShaderView::ShaderResource);

	commandList->PushConstants(shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();

	return true;
}

void st::gfx::CompositeRenderStage::OnAttached()
{
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor", 
		rapi::ResourceState::SHADER_RESOURCE, rapi::ResourceState::SHADER_RESOURCE);
	m_SceneColor = m_RenderView->GetTexture("SceneColor");

	m_BlitPSO = m_RenderView->GetDeviceManager()->GetCommonResources()->GetBlitPSO(
		m_RenderView->GetFramebuffer()->GetFramebufferInfo());
}

void st::gfx::CompositeRenderStage::OnDetached()
{
	assert(0); // TODO
}