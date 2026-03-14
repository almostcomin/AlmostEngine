#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Interop/RenderResources.h"

void st::gfx::DebugRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_TonemappedTexture = builder.GetTextureHandle("ToneMapped");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	builder.AddTextureDependency(m_TonemappedTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void st::gfx::DebugRenderStage::Render(st::rhi::CommandListHandle commandList)
{
	const auto* scene = GetScene();
	if (!scene)
		return;

	if (!any(m_RenderBBoxes))
		return;

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));
	
	for (int i = 0; i < (int)BoundsType::_Size; ++i)
	{
		if (m_RenderBBoxes[i])
		{
			auto [bboxBufferDI, bboxCount] = GetAABBOXBuffer(scene, (BoundsType)i, commandList);

			interop::DebugStage shaderConstants;
			shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
			shaderConstants.aaboxDI = bboxBufferDI;
			commandList->PushGraphicsConstants(shaderConstants);

			commandList->DrawInstanced(24, bboxCount, 0);
		}
	}

	commandList->EndRenderPass();
};

void st::gfx::DebugRenderStage::OnAttached()
{
	// Create Framebuffer
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_TonemappedTexture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = GetDeviceManager()->GetDevice()->CreateFramebuffer(fbDesc, "DebugRenderStage");
	}

	// Load shaders
	{
		m_VS = GetDeviceManager()->GetShaderFactory()->LoadShader(
			"AABOX_vs", rhi::ShaderType::Vertex);
		m_PS = GetDeviceManager()->GetShaderFactory()->LoadShader(
			"AABOX_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rhi::GraphicsPipelineStateDesc desc{
			.VS = m_VS.get_weak(),
			.PS = m_PS.get_weak(),
			.blendState {},
			.depthStencilState = {
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthFunc = rhi::ComparisonFunc::Greater },
			.rasterState = {},
			.primTopo = rhi::PrimitiveTopology::LineList
		};

		m_PSO = GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
			desc, m_FB->GetFramebufferInfo(), "DebugRenderStage");
	}
}

void st::gfx::DebugRenderStage::OnDetached()
{
	m_PSO.reset();
	m_VS.reset();
	m_PS.reset();
	m_AABBOXBuffer.reset();
	m_FB.reset();
}

void st::gfx::DebugRenderStage::OnBackbufferResize()
{
	// Re-create fb
	{
		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderGraph->GetTexture(m_TonemappedTexture))
			.SetDepthAttachment(m_RenderGraph->GetTexture(m_SceneDepthTexture));
		m_FB = GetDeviceManager()->GetDevice()->CreateFramebuffer(fbDesc, "DebugRenderStage");
	}

	// Re-create PSO
	{
		rhi::GraphicsPipelineStateDesc desc{
			.VS = m_VS.get_weak(),
			.PS = m_PS.get_weak(),
			.blendState {},
			.depthStencilState = {
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthFunc = rhi::ComparisonFunc::Greater },
			.rasterState = {},
			.primTopo = rhi::PrimitiveTopology::LineList
		};

		m_PSO = GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
			desc, m_FB->GetFramebufferInfo(), "DebugRenderStage");
	}
}

std::pair<st::rhi::BufferReadOnlyView, size_t> st::gfx::DebugRenderStage::GetAABBOXBuffer(const Scene* scene, BoundsType boundsType, rhi::CommandListHandle commandList)
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	// Check the number of aabox we need
	if (!scene)
		return { rhi::c_InvalidDescriptorIndex, 0 };
	 
	// Collect all bboxes
	std::vector<st::math::aabox3f> aabboxes;
	st::gfx::SceneGraph::Walker walker{ *scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (node->HasBounds(boundsType))
		{
			aabboxes.push_back(node->GetWorldBounds(boundsType));
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}

	if (aabboxes.empty())
		return { rhi::c_InvalidDescriptorIndex, 0 };

	if (!m_AABBOXBuffer || (m_AABBOXBuffer->GetDesc().sizeBytes < (aabboxes.size() * sizeof(interop::AABB))))
	{
		if (m_AABBOXBuffer)
		{
			device->ReleaseQueued(std::move(m_AABBOXBuffer));
		}

		m_AABBOXBuffer = device->CreateBuffer(
			rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Upload,
				.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
				.sizeBytes = aabboxes.size() * sizeof(interop::AABB),
				.stride = sizeof(interop::AABB) },
			rhi::ResourceState::SHADER_RESOURCE,
			"AABBOX buffer");
	}

	void *ptr = m_AABBOXBuffer->Map();
	std::memcpy(ptr, aabboxes.data(), aabboxes.size() * sizeof(interop::AABB));
	m_AABBOXBuffer->Unmap();

	return { m_AABBOXBuffer->GetReadOnlyView(), aabboxes.size() };
}