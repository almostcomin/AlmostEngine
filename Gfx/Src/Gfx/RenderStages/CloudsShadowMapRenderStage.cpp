#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/CloudsShadowMapRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

alm::gfx::CloudsShadowmapRenderStage::CloudsShadowmapRenderStage()
{
}

void alm::gfx::CloudsShadowmapRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_CloudsShadowmapTexture = builder.CreateTexture("CloudsShadowmap", RenderGraph::TextureResourceType::ShaderResource, 1024, 1024, 1,
		rhi::Format::R16_FLOAT, true);
	m_LinearDepthTexture = builder.GetTextureHandle("LinearDepth");

	builder.AddTextureDependency(m_CloudsShadowmapTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddTextureDependency(m_LinearDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::CloudsShadowmapRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!GetScene())
		return;
	if (!GetCamera())
		return;

	uint2 dstTextureSize = m_RenderGraph->GetTexture2dDimensions(m_CloudsShadowmapTexture);

	commandList->BeginMarker("CloudsShadowmap");

	commandList->SetPipelineState(m_PSO.get());

	auto* cloudsShadowmapData = (interop::CloudsShadowmapData*)m_CloudsShadowmapCB.Map();

	cloudsShadowmapData->DstTextureDI = m_RenderGraph->GetTextureStorageView(m_CloudsShadowmapTexture);
	cloudsShadowmapData->LinearDepthTexDI = m_RenderGraph->GetTextureSampledView(m_LinearDepthTexture);
	//cloudsShadowmapData->CloudsBaseShapeTexture = ...
	cloudsShadowmapData->DstTextureSize = dstTextureSize;
	cloudsShadowmapData->MatClipToTranslatedWorld = GetCamera()->GetClipToTranslatedWorldMatrix();
	cloudsShadowmapData->CameraForward = GetCamera()->GetForward();

	m_CloudsShadowmapCB.Unmap();

	interop::CloudsShadowmapConstants shaderConstants;
	shaderConstants.CloudsShadowmapDataDI = m_CloudsShadowmapCB.GetUniformView();

	commandList->PushComputeConstants(0, shaderConstants);

	commandList->Dispatch(DivRoundUp(dstTextureSize.x, 16u), DivRoundUp(dstTextureSize.y, 16u), 1);

	commandList->EndMarker();
}

void alm::gfx::CloudsShadowmapRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* shaderFactory = deviceManager->GetShaderFactory();
	auto* device = deviceManager->GetDevice();

	m_CS = shaderFactory->LoadShader("CloudsShadowmap_cs", rhi::ShaderType::Compute);
	m_PSO = device->CreateComputePipelineState({ .CS = m_CS.get_weak() }, "CloudsShadowmap");

	m_CloudsShadowmapCB.InitUniformBuffer(sizeof(interop::CloudsShadowmapData), deviceManager, "CloudsShadowmapConstantBuffer");
}

void alm::gfx::CloudsShadowmapRenderStage::OnDetached()
{
	m_CloudsShadowmapCB.Release();

	m_PSO.reset();
	m_CS.reset();
}