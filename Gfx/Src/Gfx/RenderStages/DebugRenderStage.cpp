#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Interop/RenderResources.h"

void st::gfx::DebugRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
		return;

	if (!m_RenderBBoxes)
		return;

	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));
	
	auto [bboxBufferDI, bboxCount] = GetAABBOXBuffer(scene.get(), commandList);

	interop::DebugStage shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneConstantBufferDI();
	shaderConstants.aaboxDI = bboxBufferDI;	
	commandList->PushGraphicsConstants(shaderConstants);

	commandList->DrawInstanced(24, bboxCount);

	commandList->EndRenderPass();
};

void st::gfx::DebugRenderStage::OnAttached()
{
	// Create Framebuffer
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ToneMapped",
			rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth",
			rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);

		auto fbDesc = rhi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("ToneMapped"))
			.SetDepthAttachment(m_RenderView->GetTexture("SceneDepth"));
		m_FB = m_RenderView->GetDeviceManager()->GetDevice()->CreateFramebuffer(fbDesc, "DebugRenderStage");
	}

	// Load shaders
	{
		m_VS = m_RenderView->GetDeviceManager()->GetShaderFactory()->LoadShader(
			"AABOX_vs", rhi::ShaderType::Vertex);
		m_PS = m_RenderView->GetDeviceManager()->GetShaderFactory()->LoadShader(
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

		m_PSO = m_RenderView->GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
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
			.AddColorAttachment(m_RenderView->GetTexture("ToneMapped"))
			.SetDepthAttachment(m_RenderView->GetTexture("SceneDepth"));
		m_FB = m_RenderView->GetDeviceManager()->GetDevice()->CreateFramebuffer(fbDesc, "DebugRenderStage");
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

		m_PSO = m_RenderView->GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
			desc, m_FB->GetFramebufferInfo(), "DebugRenderStage");
	}
}

std::pair<st::rhi::DescriptorIndex, size_t> st::gfx::DebugRenderStage::GetAABBOXBuffer(const Scene* scene, rhi::CommandListHandle commandList)
{
	rhi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Check the number of aabox we need
	if (!scene)
		return { rhi::c_InvalidDescriptorIndex, 0 };

	// Collect all bboxes
	std::vector<st::math::aabox3f> aabboxes;
	st::gfx::SceneGraph::Walker walker{ *scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (node->HasBounds())
		{
			aabboxes.push_back(node->GetWorldBounds());
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
			device->ReleaseQueued(m_AABBOXBuffer);
		}

		m_AABBOXBuffer = device->CreateBuffer(
			rhi::BufferDesc{
				.memoryAccess = rhi::MemoryAccess::Upload,
				.shaderUsage = rhi::BufferShaderUsage::ShaderResource,
				.sizeBytes = aabboxes.size() * sizeof(interop::AABB),
				.stride = sizeof(interop::AABB) },
			rhi::ResourceState::SHADER_RESOURCE,
			"AABBOX buffer");
	}

	void *ptr = m_AABBOXBuffer->Map();
	std::memcpy(ptr, aabboxes.data(), aabboxes.size() * sizeof(interop::AABB));
	m_AABBOXBuffer->Unmap();

	return { m_AABBOXBuffer->GetShaderViewIndex(rhi::BufferShaderView::ShaderResource), aabboxes.size() };
}