#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderView.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/UploadBuffer.h"
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
	UploadBuffer* uploadBuffer = deviceManager->GetUploadBuffer();
	const alm::gfx::GpuSceneBuffers* gpuSceneBuffers = deviceManager->GetGpuSceneBuffers();
	alm::gfx::Scene* scene = GetScene();
	if (!scene)
	{
		return;
	}
	const bool statsEnabled = (m_StatsRequests != 0);

	// Init Stats
	// The stats kernel accumulates with atomics, so the reset is required every frame
	// while stats are enabled. Gated on the request flag: the RETRIEVE_STATS shader define
	// and this flag must stay in sync.
	if (statsEnabled)
	{
		commandList->BeginMarker("Init Stats");

		auto [stats, offset] = uploadBuffer->RequestSpaceForBufferDataUpload<interop::CullingStats>();

		stats->CameraTotalInstances = 0;
		stats->CameraVisibleInstances = 0;
		stats->CameraTotalPrimitives = 0;
		stats->CameraVisiblePrimitives = 0;
		stats->ShadowTotalInstances = 0;
		stats->ShadowVisibleInstances = 0;
		stats->ShadowTotalPrimitives = 0;
		stats->ShadowVisiblePrimitives = 0;

		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST));
		commandList->CopyBufferToBuffer(m_StatsBuffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, sizeof(interop::CullingStats));
		commandList->PushBarrier(rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::UNORDERED_ACCESS));

		commandList->EndMarker();
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
	shaderConstants.StatsBufferDI = m_StatsBuffer->GetReadWriteView();

	commandList->PushComputeConstants(0, shaderConstants);

	// Prepare
	commandList->SetPipelineState(m_PreparePSO.get());
	commandList->Dispatch(DivRoundUp(batchCount, 256u), 1, 1);

	commandList->PushBarriers({
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseShadowArgsBuffer).get()) });

	// Cull — the stats variant accumulates per-instance stats (RETRIEVE_STATS 1)
	
	commandList->SetPipelineState((statsEnabled ? m_CullingStatsPSO : m_CullingPSO).get());
	commandList->Dispatch(DivRoundUp(staticInstanceCount + transientInstanceCount, 256u), 1, 1);

	commandList->PushBarriers({
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_SparseShadowArgsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_CountsBuffer).get()),
		rhi::Barrier::Memory(m_RenderGraph->GetBuffer(m_ShadowCountsBuffer).get()) });

	// Compact
	commandList->SetPipelineState(m_CompactPSO.get());
	commandList->Dispatch(DivRoundUp(batchCount, 256u), 1, 1);

	// Retrieve stats
	if (statsEnabled)
	{
		commandList->PushBarriers({
			rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::COPY_SRC),
			rhi::Barrier::Buffer(m_StatsBufferRB.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST) });

		commandList->CopyBufferToBuffer(m_StatsBufferRB.get(), 0, m_StatsBuffer.get(), 0, sizeof(interop::CullingStats));

		commandList->PushBarriers({
			rhi::Barrier::Buffer(m_StatsBufferRB.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::COMMON),
			rhi::Barrier::Buffer(m_StatsBuffer.get(), rhi::ResourceState::COPY_SRC, rhi::ResourceState::COMMON) });
	}
}

void alm::gfx::GPUCullingRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
	rhi::Device* device = deviceManager->GetDevice();

	m_PrepareCS = shaderFactory->LoadShader("CullingPrepare_cs", rhi::ShaderType::Compute);
	m_CullingCS = shaderFactory->LoadShader("Culling_cs", rhi::ShaderType::Compute);
	m_CullingStatsCS = shaderFactory->LoadShader("CullingStats_cs", rhi::ShaderType::Compute);
	m_CompactCS = shaderFactory->LoadShader("CullingCompact_cs", rhi::ShaderType::Compute);

	m_PreparePSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_PrepareCS.get_weak() }, "GPUCullingPrepare");
	m_CullingPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CullingCS.get_weak() }, "GPUCulling");
	m_CullingStatsPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CullingStatsCS.get_weak() }, "GPUCullingStats");
	m_CompactPSO = device->CreateComputePipelineState(rhi::ComputePipelineStateDesc{
		.CS = m_CompactCS.get_weak() }, "GPUCullingCompact");

	// Stats
	{
		{
			auto desc = rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::ReadWrite,
				.sizeBytes = sizeof(interop::CullingStats),
				.format = rhi::Format::UNKNOWN,
				.stride = sizeof(interop::CullingStats) };

			m_StatsBuffer = device->CreateBuffer(desc, rhi::ResourceState::COMMON, "CullingStats");
		}
		{
			auto desc = rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Readback,
				.shaderUsage = rhi::BufferShaderUsage::None,
				.sizeBytes = sizeof(interop::CullingStats),
				.format = rhi::Format::UNKNOWN,
				.stride = 0 };

			m_StatsBufferRB = device->CreateBuffer(desc, rhi::ResourceState::COMMON, "CullingStatsRB");

			// Readback buffers are persistently mapped: cache the pointer for GetStats()
			m_StatsBufferRBPtr = (interop::CullingStats*)m_StatsBufferRB->Map();

			// Defined content before the first readback lands (1-frame-late stats)
			if (m_StatsBufferRBPtr)
				*m_StatsBufferRBPtr = {};
		}
	}
}

void alm::gfx::GPUCullingRenderStage::OnDetached()
{
	m_StatsBufferRBPtr = nullptr;
	m_StatsBufferRB.reset();
	m_StatsBuffer.reset();

	m_CompactPSO.reset();
	m_CullingStatsPSO.reset();
	m_CullingPSO.reset();
	m_PreparePSO.reset();

	m_CompactCS.reset();
	m_CullingStatsCS.reset();
	m_CullingCS.reset();
	m_PrepareCS.reset();

	m_StatsRequests = 0;
}

void alm::gfx::GPUCullingRenderStage::RequestStats()
{
	++m_StatsRequests;
}

void alm::gfx::GPUCullingRenderStage::UnrequestStats()
{
	assert(m_StatsRequests > 0 && "UnrequestStats called without a matching RequestStats");
	--m_StatsRequests;
}

interop::CullingStats alm::gfx::GPUCullingRenderStage::GetStats() const
{
	if (m_StatsRequests == 0)
	{
		LOG_WARNING("GetStats() called while culling stats are not active. Call RequestStats() first.");
		return {};
	}

	// The readback buffer is persistently mapped; its content corresponds to the previous frame
	// (same 1-frame-late semantics as the timer queries).
	return *m_StatsBufferRBPtr;
}