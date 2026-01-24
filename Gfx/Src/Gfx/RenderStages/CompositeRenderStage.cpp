#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void st::gfx::CompositeRenderStage::Render()
{
	auto commandList = m_RenderView->GetCommandList();

	auto fb = m_RenderView->GetFramebuffer();
	commandList->BeginRenderPass(
		fb.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		{},
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)fb->GetFramebufferInfo().width, (float)fb->GetFramebufferInfo().height }));

	interop::PresentConstants shaderConstants;
	shaderConstants.sceneTextureDI = m_RenderView->GetTexture("SceneColor")->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
	shaderConstants.colorSpace = (uint)m_RenderView->GetDeviceManager()->GetColorSpace();
	shaderConstants.exposure = m_Exposure;

	commandList->PushConstants(shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void st::gfx::CompositeRenderStage::OnAttached()
{
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneColor",
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = m_RenderView->GetDeviceManager()->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("Present_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		st::gfx::CommonResources* commonResurces = m_RenderView->GetDeviceManager()->GetCommonResources();
		m_PSO = commonResurces->CreateBlitPSO(
			m_RenderView->GetFramebuffer()->GetFramebufferInfo(),
			commonResurces->GetBlitVS(),
			m_PS.get_weak(),
			"Composite Render Stage");
	}
}

void st::gfx::CompositeRenderStage::OnDetached()
{
	st::rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_PSO);
	device->ReleaseQueued(m_PS);
}

void st::gfx::CompositeRenderStage::OnBackbufferResize()
{
	// Recreate PSO
	{
		st::gfx::CommonResources* commonResurces = m_RenderView->GetDeviceManager()->GetCommonResources();
		m_PSO = commonResurces->CreateBlitPSO(
			m_RenderView->GetFramebuffer()->GetFramebufferInfo(),
			commonResurces->GetBlitVS(),
			m_PS.get_weak(),
			"Composite Render Stage");
	}
}