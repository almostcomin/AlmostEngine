#include "Gfx/RenderStages/LinearizeDepthRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderGraphBuilder.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void alm::gfx::LinearizeDepthRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_LinearDepthTexture = builder.CreateTexture("LinearDepth", RenderGraph::TextureResourceType::ShaderResource,
		alm::gfx::RenderGraph::c_BBSize, alm::gfx::RenderGraph::c_BBSize, 1, rhi::Format::R32_FLOAT, true);
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);

	builder.AddTextureDependency(m_LinearDepthTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
}

void alm::gfx::LinearizeDepthRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	const auto& camera = GetRenderView()->GetCamera();
	if (!camera)
		return;

	auto depthTex = m_RenderGraph->GetTexture(m_SceneDepthTexture);
	const auto& desc = depthTex->GetDesc();

	commandList->SetPipelineState(m_LinearizeDepthPSO.get());
	
	interop::LinearizeDepthConstants shaderConstants;
	shaderConstants.srcDepthTexDI = m_RenderGraph->GetTextureSampledView(m_SceneDepthTexture);
	shaderConstants.outLinearDepthTexDI = m_RenderGraph->GetTextureStorageView(m_LinearDepthTexture);
	shaderConstants.width = desc.width;
	shaderConstants.height = desc.height;
	shaderConstants.nearPlaneDist = camera->GetZNear();

	commandList->PushComputeConstants(0, shaderConstants);
	commandList->Dispatch(DivRoundUp(desc.width, 16u), DivRoundUp(desc.height, 16u), 1);
}

void alm::gfx::LinearizeDepthRenderStage::OnAttached()
{
	DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();

	// Load shaders
	{
		alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_LinearizeDepthCS = shaderFactory->LoadShader("LinearizeDepth_cs", rhi::ShaderType::Compute);
	}

	// Create PSOs
	{
		m_LinearizeDepthPSO = device->CreateComputePipelineState({ m_LinearizeDepthCS.get_weak() }, "LinearizeDepthPSO");
	}
}

void alm::gfx::LinearizeDepthRenderStage::OnDetached()
{
	m_LinearizeDepthPSO.reset();
	m_LinearizeDepthCS.reset();
}