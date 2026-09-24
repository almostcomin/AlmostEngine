#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderView.h"
#include "Gfx/GpuSceneBuffers.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"

void alm::gfx::GPUCullingRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_IndirectArgsBuffer = builder.CreateBuffer("IndirectArgsBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite | rhi::BufferShaderUsage::IndirectArguments,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::IndirectDrawCommand),
		.stride = sizeof(interop::IndirectDrawCommand) });

	m_PayloadBuffer = builder.CreateBuffer("PayloadBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::VisibleInstancePayload),
		.stride = sizeof(interop::VisibleInstancePayload) });

	m_ShadowIndirectArgsBuffer = builder.CreateBuffer("ShadowIndirectArgsBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite | rhi::BufferShaderUsage::IndirectArguments,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::IndirectDrawCommand),
		.stride = sizeof(interop::IndirectDrawCommand) });

	m_ShadowPayloadBuffer = builder.CreateBuffer("ShadowPayloadBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::VisibleInstancePayload),
		.stride = sizeof(interop::VisibleInstancePayload) });

	builder.AddBufferDependency(m_IndirectArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_PayloadBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_ShadowIndirectArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_ShadowPayloadBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
}

void alm::gfx::GPUCullingRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	const alm::gfx::GpuSceneBuffers* gpuSceneBuffers = deviceManager->GetGpuSceneBuffers();
	alm::gfx::Scene* scene = GetScene();
	if (!scene)
	{
		return;
	}

	const uint32_t batchCount = gpuSceneBuffers->GetBatchTableSize(scene->GetGpuSceneBuffersHandle());
	const uint32_t instanceCount = gpuSceneBuffers->GetRenderInstancesCount(scene->GetGpuSceneBuffersHandle());

	interop::CullingConstants shaderConstants;
	shaderConstants.SceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.CameraArgsDI = m_RenderGraph->GetBufferReadWriteView(m_IndirectArgsBuffer);
	shaderConstants.CameraPayloadDI = m_RenderGraph->GetBufferReadWriteView(m_PayloadBuffer);
	shaderConstants.ShadowArgsDI = m_RenderGraph->GetBufferReadWriteView(m_ShadowIndirectArgsBuffer);
	shaderConstants.ShadowPayloadDI = m_RenderGraph->GetBufferReadWriteView(m_ShadowPayloadBuffer);
	shaderConstants.BatchTableDI = gpuSceneBuffers->GetBatchTableBufferView(scene->GetGpuSceneBuffersHandle());
	shaderConstants.CullDataDI = gpuSceneBuffers->GetInstancesCullDataBufferView(scene->GetGpuSceneBuffersHandle());
	shaderConstants.BatchCount = batchCount;
	shaderConstants.InstanceCount = instanceCount;
	shaderConstants.ShadowEnabled = GetRenderView()->IsShadowmapValid();

	commandList->PushComputeConstants(0, shaderConstants);

	commandList->SetPipelineState(m_PreparePSO.get());
	commandList->Dispatch(DivRoundUp(batchCount, 256u), 1, 1);

	commandList->PushBarriers({
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_IndirectArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_ShadowIndirectArgsBuffer).get()) });

	commandList->SetPipelineState(m_CullingPSO.get());
	commandList->Dispatch(DivRoundUp(instanceCount, 256u), 1, 1);
}

void alm::gfx::GPUCullingRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
	rhi::Device* device = deviceManager->GetDevice();

	m_PrepareCS = shaderFactory->LoadShader("CullingPrepare_cs", rhi::ShaderType::Compute);
	m_CullingCS = shaderFactory->LoadShader("Culling_cs", rhi::ShaderType::Compute);

	m_PreparePSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_PrepareCS.get_weak() }, "GPUCullingPrepare");
	m_CullingPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CullingCS.get_weak() }, "GPUCulling");
}

void alm::gfx::GPUCullingRenderStage::OnDetached()
{
	m_PreparePSO.reset();
	m_CullingPSO.reset();
	m_CullingCS.reset();
	m_PrepareCS.reset();
}