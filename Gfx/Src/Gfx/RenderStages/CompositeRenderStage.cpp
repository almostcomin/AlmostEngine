#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void alm::gfx::CompositeRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_ToneMappedTexture = builder.GetTextureHandle("ToneMapped");
	m_ImGuiTexture = builder.GetTextureHandle("ImGui");

	builder.AddTextureDependency(m_ToneMappedTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	builder.AddTextureDependency(m_ImGuiTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	builder.AddRenderTargetWriteDependency();
}

void alm::gfx::CompositeRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	auto fb = m_RenderGraph->GetFramebuffer();
	commandList->BeginRenderPass(
		fb.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store, rhi::ClearValue::ColorBlack()} },
		{},
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::CompositeConstants shaderConstants;
	shaderConstants.sceneTextureDI = m_RenderGraph->GetTextureSampledView(m_ToneMappedTexture);
	shaderConstants.uiTextureDI = m_RenderGraph->GetTextureSampledView(m_ImGuiTexture);
	shaderConstants.colorSpace = (uint)m_RenderGraph->GetDeviceManager()->GetColorSpace();
	shaderConstants.paperWhiteNits = m_PaperWhiteNits;

	commandList->PushGraphicsConstants(0, shaderConstants);

	commandList->Draw(3);

	commandList->EndRenderPass();
}

void alm::gfx::CompositeRenderStage::OnAttached()
{
	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = m_RenderGraph->GetDeviceManager()->GetShaderFactory();
		m_PS = shaderFactory->LoadShader("Composite_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		alm::gfx::CommonResources* commonResurces = m_RenderGraph->GetDeviceManager()->GetCommonResources();
		m_PSO = commonResurces->CreateFullscreenPassPSO(
			m_RenderGraph->GetFramebuffer()->GetFramebufferInfo(),
			m_PS.get_weak(),
			"Composite Render Stage");
	}
}

void alm::gfx::CompositeRenderStage::OnDetached()
{
	alm::rhi::Device* device = m_RenderGraph->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(std::move(m_PSO));
	device->ReleaseQueued(std::move(m_PS));
}

void alm::gfx::CompositeRenderStage::OnBackbufferResize()
{
	// Recreate PSO
	{
		alm::gfx::CommonResources* commonResurces = m_RenderGraph->GetDeviceManager()->GetCommonResources();
		m_PSO = commonResurces->CreateFullscreenPassPSO(
			m_RenderGraph->GetFramebuffer()->GetFramebufferInfo(),
			m_PS.get_weak(),
			"Composite Render Stage");
	}
}