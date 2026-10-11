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
	{
		rhi::BufferDesc desc{
			.shaderUsage = rhi::BufferShaderUsage::ReadWrite | rhi::BufferShaderUsage::IndirectArguments,
			.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::IndirectDrawCommand),
			.stride = sizeof(interop::IndirectDrawCommand) };

		m_IndirectDrawArgsBuffer = builder.CreateBuffer("IndirectDrawArgsBuffer", desc);
		m_ShadowIndirectDrawArgsBuffer = builder.CreateBuffer("ShadowIndirectDrawArgsBuffer", desc);
		m_SparseArgsBuffer = builder.CreateBuffer("SparseIndirectDrawArgsBuffer", desc);
		m_SparseShadowArgsBuffer = builder.CreateBuffer("SparseShadowIndirectDrawArgsBuffer", desc);
	}

	{
		rhi::BufferDesc desc{
			.shaderUsage = rhi::BufferShaderUsage::ReadWrite | rhi::BufferShaderUsage::IndirectArguments,
			.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(uint32_t),
			.stride = sizeof(uint32_t) };

		m_CountsBuffer = builder.CreateBuffer("IndirectDrawCountsBuffer", desc);
		m_ShadowCountsBuffer = builder.CreateBuffer("ShadowIndirectDrawCountsBuffer", desc);
	}

	m_PayloadBuffer = builder.CreateBuffer("PayloadBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::VisibleInstancePayload),
		.stride = sizeof(interop::VisibleInstancePayload) });

	m_ShadowPayloadBuffer = builder.CreateBuffer("ShadowPayloadBuffer", rhi::BufferDesc{
		.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
		.sizeBytes = GpuSceneBuffers::MaxInstances() * sizeof(interop::VisibleInstancePayload),
		.stride = sizeof(interop::VisibleInstancePayload) });

	builder.AddBufferDependency(m_IndirectDrawArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_SparseArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_ShadowIndirectDrawArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_SparseShadowArgsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_CountsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_ShadowCountsBuffer, RenderGraph::AccessMode::Write,
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::UNORDERED_ACCESS);
	builder.AddBufferDependency(m_PayloadBuffer, RenderGraph::AccessMode::Write,
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
	const uint32_t staticInstanceCount = gpuSceneBuffers->GetStaticInstanceScanCount(scene->GetGpuSceneBuffersHandle());
	const uint32_t transientInstanceCount = gpuSceneBuffers->GetTransientInstancesCount(scene->GetGpuSceneBuffersHandle());

	interop::CullingConstants shaderConstants;
	shaderConstants.SceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.SparseCameraArgsDI = m_RenderGraph->GetBufferReadWriteView(m_SparseArgsBuffer);
	shaderConstants.CompactedCameraArgsDI = m_RenderGraph->GetBufferReadWriteView(m_IndirectDrawArgsBuffer);
	shaderConstants.CameraCountsDI = m_RenderGraph->GetBufferReadWriteView(m_CountsBuffer);
	shaderConstants.CameraPayloadDI = m_RenderGraph->GetBufferReadWriteView(m_PayloadBuffer);
	shaderConstants.SparseShadowArgsDI = m_RenderGraph->GetBufferReadWriteView(m_SparseShadowArgsBuffer);
	shaderConstants.CompactedShadowArgsDI = m_RenderGraph->GetBufferReadWriteView(m_ShadowIndirectDrawArgsBuffer);
	shaderConstants.ShadowCountsDI = m_RenderGraph->GetBufferReadWriteView(m_ShadowCountsBuffer);
	shaderConstants.ShadowPayloadDI = m_RenderGraph->GetBufferReadWriteView(m_ShadowPayloadBuffer);
	shaderConstants.BatchTableDI = gpuSceneBuffers->GetBatchTableBufferView(scene->GetGpuSceneBuffersHandle());
	shaderConstants.CullDataDI = gpuSceneBuffers->GetInstancesCullDataBufferView(scene->GetGpuSceneBuffersHandle());
	shaderConstants.BatchCount = batchCount;
	shaderConstants.StaticInstanceCount = staticInstanceCount;
	shaderConstants.TotalInstanceCount = staticInstanceCount + transientInstanceCount;
	shaderConstants.StaticInstanceCapacity = alm::gfx::GpuSceneBuffers::kMaxStaticInstanceCount;
	shaderConstants.ShadowEnabled = GetRenderView()->IsShadowmapValid();

	commandList->PushComputeConstants(0, shaderConstants);

	// Prepare
	commandList->SetPipelineState(m_PreparePSO.get());
	commandList->Dispatch(DivRoundUp(batchCount, 256u), 1, 1);

	commandList->PushBarriers({
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseShadowArgsBuffer).get()) });

	// Cull
	commandList->SetPipelineState(m_CullingPSO.get());
	commandList->Dispatch(DivRoundUp(staticInstanceCount + transientInstanceCount, 256u), 1, 1);

	commandList->PushBarriers({
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseShadowArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_CountsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_ShadowCountsBuffer).get()) });

	// Compact
	commandList->SetPipelineState(m_CompactPSO.get());
	commandList->Dispatch(DivRoundUp(batchCount, 256u), 1, 1);
}

void alm::gfx::GPUCullingRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
	rhi::Device* device = deviceManager->GetDevice();

	m_PrepareCS = shaderFactory->LoadShader("CullingPrepare_cs", rhi::ShaderType::Compute);
	m_CullingCS = shaderFactory->LoadShader("Culling_cs", rhi::ShaderType::Compute);
	m_CompactCS = shaderFactory->LoadShader("CullingCompact_cs", rhi::ShaderType::Compute);

	m_PreparePSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_PrepareCS.get_weak() }, "GPUCullingPrepare");
	m_CullingPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CullingCS.get_weak() }, "GPUCulling");
	m_CompactPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CompactCS.get_weak() }, "GPUCullingCompact");
}

void alm::gfx::GPUCullingRenderStage::OnDetached()
{
	m_CompactPSO.reset();
	m_CullingPSO.reset();
	m_PreparePSO.reset();

	m_CompactCS.reset();
	m_CullingCS.reset();
	m_PrepareCS.reset();
}