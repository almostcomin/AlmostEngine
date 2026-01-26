#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void st::gfx::ToneMappingRenderStage::Render()
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	commandList->SetPipelineState(m_PSO.get());

	st::rhi::TextureHandle inputTexture = m_RenderView->GetTexture("SceneColor");
	st::rhi::TextureHandle outputTexture = m_RenderView->GetTexture("ToneMapped");

	interop::TonemapConstants shaderConstants = {};
	shaderConstants.inputTextureDI = inputTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);
	shaderConstants.outputTextureDI = outputTexture->GetShaderViewIndex(rhi::TextureShaderView::UnorderedAcces);
	shaderConstants.exposure = 10.f;

	const uint32_t width = outputTexture->GetDesc().width;
	const uint32_t height = outputTexture->GetDesc().height;

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);	
}

void st::gfx::ToneMappingRenderStage::OnAttached()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	m_RenderView->CreateTexture("ToneMapped", RenderView::TextureResourceType::ShaderResource, 
		RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA16_FLOAT, true);

	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneColor", 
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ToneMapped",
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_CS = shaderFactory->LoadShader("ToneMapping_cs", rhi::ShaderType::Compute);
	}

	// Create PSO
	{
		m_PSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{ m_CS.get_weak() }, "TonemappingPSO");
	}
}

void st::gfx::ToneMappingRenderStage::OnDetached()
{
	m_PSO = nullptr;
	m_CS = nullptr;
}