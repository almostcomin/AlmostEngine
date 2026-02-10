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

	interop::CompositeConstants shaderConstants;
	shaderConstants.sceneTextureDI = m_RenderView->GetSampledView("ToneMapped");
	shaderConstants.uiTextureDI = m_RenderView->GetSampledView("ImGui");
	shaderConstants.colorSpace = (uint)m_RenderView->GetDeviceManager()->GetColorSpace();
	shaderConstants.paperWhiteNits = m_PaperWhiteNits;

	commandList->PushGraphicsConstants(shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void st::gfx::CompositeRenderStage::OnAttached()
{
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "ToneMapped",
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "ImGui",
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = m_RenderView->GetDeviceManager()->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("Composite_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		st::gfx::CommonResources* commonResurces = m_RenderView->GetDeviceManager()->GetCommonResources();
		m_PSO = commonResurces->CreateBlitGraphicsPSO(
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
		m_PSO = commonResurces->CreateBlitGraphicsPSO(
			m_RenderView->GetFramebuffer()->GetFramebufferInfo(),
			commonResurces->GetBlitVS(),
			m_PS.get_weak(),
			"Composite Render Stage");
	}
}