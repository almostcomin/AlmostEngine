#include "Gfx/RenderStages/LinearizeDepthRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void st::gfx::LinearizeDepthRenderStage::Render()
{
	const auto& camera = m_RenderView->GetCamera();
	if (!camera)
		return;

	auto depthTex = m_RenderView->GetTexture("SceneDepth");
	const auto& desc = depthTex->GetDesc();

	rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

	commandList->SetPipelineState(m_LinearizeDepthPSO.get());
	
	interop::LinearizeDepthConstants shaderConstants;
	shaderConstants.srcDepthTexDI = m_RenderView->GetTextureSampledView("SceneDepth");
	shaderConstants.outLinearDepthTexDI = m_RenderView->GetTextureStorageView("LinearDepth");
	shaderConstants.width = desc.width;
	shaderConstants.height = desc.height;
	shaderConstants.nearPlaneDist = camera->GetZNear();

	commandList->PushComputeConstants(shaderConstants);
	commandList->Dispatch(DivRoundUp(desc.width, 16u), DivRoundUp(desc.height, 16u), 1);
}

void st::gfx::LinearizeDepthRenderStage::OnAttached()
{
	DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Create resources
	{
		m_RenderView->CreateTexture("LinearDepth", RenderView::TextureResourceType::ShaderResource,
			st::gfx::RenderView::c_BBSize, st::gfx::RenderView::c_BBSize, 1, rhi::Format::R16_FLOAT, true);
	}

	// Declare resource access
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth",
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "LinearDepth",
			rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_LinearizeDepthCS = shaderFactory->LoadShader("LinearizeDepth_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_LinearizeDepthPSO = device->CreateComputePipelineState({ m_LinearizeDepthCS.get_weak() }, "LinearizeDepthPSO");
	}
}

void st::gfx::LinearizeDepthRenderStage::OnDetached()
{
	m_LinearizeDepthPSO.reset();
	m_LinearizeDepthCS.reset();
}