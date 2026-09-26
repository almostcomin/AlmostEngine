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

	auto [vertexBufferDI, vertexCount] = BuildGridGeometry();
	if (vertexCount == 0)
		return;

	interop::GridStageConstants shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.gridVerticesDI = vertexBufferDI;
	shaderConstants.fadeStartDist = m_FadeStart;
	shaderConstants.fadeEndDist = m_FadeEnd;

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

	CreatePSO();
}

void alm::gfx::GridRenderStage::OnDetached()
{
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

std::pair<alm::rhi::BufferReadOnlyView, size_t> alm::gfx::GridRenderStage::BuildGridGeometry()
{
	const Camera* camera = GetCamera();
	if (!camera)
		return { rhi::c_InvalidDescriptorIndex, 0 };

	const float3& camPos = camera->GetPosition();
	const int fromX = (int)floorf((camPos.x - m_Extent) / m_MinorStep);
	const int toX = (int)ceilf((camPos.x + m_Extent) / m_MinorStep);
	const int fromZ = (int)floorf((camPos.z - m_Extent) / m_MinorStep);
	const int toZ = (int)ceilf((camPos.z + m_Extent) / m_MinorStep);

	std::vector<interop::GridVertex> vertices;
	vertices.reserve((size_t)((toX - fromX) + (toZ - fromZ) + 2) * 2);

	// Lines parallel to the Z axis (constant X)
	for (int i = fromX; i <= toX; ++i)
	{
		const float x = (float)i * m_MinorStep;
		uint colorIdx = (i % m_MajorStep == 0) ? 1 : 0;
		if (i == 0)
			colorIdx = 3; // Z axis
		vertices.push_back({ float3(x, 0.f, (float)fromZ * m_MinorStep), colorIdx });
		vertices.push_back({ float3(x, 0.f, (float)toZ * m_MinorStep), colorIdx });
	}

	// Lines parallel to the X axis (constant Z)
	for (int i = fromZ; i <= toZ; ++i)
	{
		const float z = (float)i * m_MinorStep;
		uint colorIdx = (i % m_MajorStep == 0) ? 1 : 0;
		if (i == 0)
			colorIdx = 2; // X axis
		vertices.push_back({ float3((float)fromX * m_MinorStep, 0.f, z), colorIdx });
		vertices.push_back({ float3((float)toX * m_MinorStep, 0.f, z), colorIdx });
	}

	rhi::Device* device = GetDeviceManager()->GetDevice();

	auto buffer = device->CreateBuffer(
		rhi::BufferDesc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = vertices.size() * sizeof(interop::GridVertex),
			.stride = sizeof(interop::GridVertex) },
			rhi::ResourceState::SHADER_RESOURCE,
			"Grid vertices buffer");

	auto* ptr = (interop::GridVertex*)buffer->Map();
	memcpy(ptr, vertices.data(), vertices.size() * sizeof(interop::GridVertex));
	buffer->Unmap();

	return { buffer->GetReadOnlyView(), vertices.size() };
}
