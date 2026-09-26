#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/GridRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/Camera.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

#include <cmath>

void alm::gfx::GridRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_TonemappedTexture = builder.GetTextureHandle("ToneMapped");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	m_FB = builder.RequestFramebuffer({ m_TonemappedTexture }, m_SceneDepthTexture);

	builder.AddTextureDependency(m_TonemappedTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void alm::gfx::GridRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!m_Visible)
		return;

	EnsureFrameBuffers();

	// Refill the current frame's vertex buffer (persistent, one per frame in flight)
	rhi::BufferOwner& vertexBuffer = m_FrameVertexBuffers[GetDeviceManager()->GetFrameModuleIndex()];
	if (!vertexBuffer)
		return;

	const size_t maxVertexCount = m_BufferSizeBytes / sizeof(interop::GridVertex);
	const size_t vertexCount = BuildGridGeometry((interop::GridVertex*)vertexBuffer->Map(), maxVertexCount);
	vertexBuffer->Unmap();
	if (vertexCount == 0)
		return;

	interop::GridStageConstants shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.gridVerticesDI = vertexBuffer->GetReadOnlyView();
	shaderConstants.cutDistance = m_CutDistance;

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::NoAccess },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_RenderResources.PSO.get());
	commandList->PushGraphicsConstants(0, shaderConstants);
	commandList->Draw((uint32_t)vertexCount);

	commandList->EndRenderPass();
}

void alm::gfx::GridRenderStage::OnAttached()
{
	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* shaderFactory = deviceManager->GetShaderFactory();

	if (!m_RenderResources.VS)
	{
		m_RenderResources.VS = shaderFactory->LoadShader("Grid_vs", rhi::ShaderType::Vertex);
	}
	if (!m_RenderResources.PS)
	{
		m_RenderResources.PS = shaderFactory->LoadShader("Grid_ps", rhi::ShaderType::Pixel);
	}

	EnsureFrameBuffers();
	CreatePSO();
}

void alm::gfx::GridRenderStage::OnDetached()
{
	m_FrameVertexBuffers.clear();
	m_BufferSizeBytes = 0;

	m_RenderResources.PSO.reset();
	m_RenderResources.VS.reset();
	m_RenderResources.PS.reset();
}

void alm::gfx::GridRenderStage::OnBackbufferResize()
{
	CreatePSO();
}

void alm::gfx::GridRenderStage::CreatePSO()
{
	rhi::BlendState blendState;
	blendState.renderTarget[0].blendEnable = true;

	rhi::GraphicsPipelineStateDesc desc{
		.VS = m_RenderResources.VS.get_weak(),
		.PS = m_RenderResources.PS.get_weak(),
		.blendState = blendState,
		.depthStencilState = {
			.depthTestEnable = true,
			.depthWriteEnable = false,
			.depthFunc = rhi::ComparisonFunc::Greater },
		.rasterState = {},
		.primTopo = rhi::PrimitiveTopology::LineList };

	m_RenderResources.PSO = GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
		desc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "GridRenderStage");
}

size_t alm::gfx::GridRenderStage::ComputeRequiredBufferSize() const
{
	// Lines per direction: toX - fromX + 1, with fromX = floor((pos - extent) / step) and
	// toX = ceil((pos + extent) / step). Worst case: ceil(2 * extent / step) + 3 lines.
	// Two vertices per line, two directions (X-parallel and Z-parallel).
	const float linesPerDir = ceilf(2.f * m_Extent / m_MinorStep) + 3.f;
	return size_t(4.f * linesPerDir) * sizeof(interop::GridVertex);
}

void alm::gfx::GridRenderStage::EnsureFrameBuffers()
{
	const size_t requiredSize = ComputeRequiredBufferSize();
	if (m_BufferSizeBytes == requiredSize && !m_FrameVertexBuffers.empty())
		return;

	auto* deviceManager = m_RenderGraph->GetDeviceManager();
	auto* device = deviceManager->GetDevice();

	m_FrameVertexBuffers.clear();
	m_FrameVertexBuffers.resize(deviceManager->GetFramesInFlightCount());
	for (auto& buffer : m_FrameVertexBuffers)
	{
		buffer = device->CreateBuffer(
			rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Upload,
				.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
				.sizeBytes = requiredSize,
				.stride = sizeof(interop::GridVertex) },
				rhi::ResourceState::SHADER_RESOURCE,
				"Grid vertices buffer");
	}

	m_BufferSizeBytes = requiredSize;
}

size_t alm::gfx::GridRenderStage::BuildGridGeometry(interop::GridVertex* dst, size_t maxVertexCount)
{
	const Camera* camera = GetCamera();
	if (!camera)
		return 0;

	const float3& camPos = camera->GetPosition();
	const int fromX = (int)floorf((camPos.x - m_Extent) / m_MinorStep);
	const int toX = (int)ceilf((camPos.x + m_Extent) / m_MinorStep);
	const int fromZ = (int)floorf((camPos.z - m_Extent) / m_MinorStep);
	const int toZ = (int)ceilf((camPos.z + m_Extent) / m_MinorStep);

	size_t vertexCount = 0;

	// Lines parallel to the Z axis (constant X)
	for (int i = fromX; i <= toX && (vertexCount + 2) <= maxVertexCount; ++i)
	{
		const float x = (float)i * m_MinorStep;
		uint colorIdx = (i % m_MajorStep == 0) ? 1 : 0;
		if (i == 0)
			colorIdx = 3; // Z axis
		dst[vertexCount++] = { float3(x, 0.f, (float)fromZ * m_MinorStep), colorIdx };
		dst[vertexCount++] = { float3(x, 0.f, (float)toZ * m_MinorStep), colorIdx };
	}

	// Lines parallel to the X axis (constant Z)
	for (int i = fromZ; i <= toZ && (vertexCount + 2) <= maxVertexCount; ++i)
	{
		const float z = (float)i * m_MinorStep;
		uint colorIdx = (i % m_MajorStep == 0) ? 1 : 0;
		if (i == 0)
			colorIdx = 2; // X axis
		dst[vertexCount++] = { float3((float)fromX * m_MinorStep, 0.f, z), colorIdx };
		dst[vertexCount++] = { float3((float)toX * m_MinorStep, 0.f, z), colorIdx };
	}

	return vertexCount;
}
