#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RenderAPI/Device.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Interop/RenderResources.h"

bool st::gfx::DebugRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
		return true;

	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rapi::RenderPassOp{rapi::RenderPassOp::LoadOp::Load, rapi::RenderPassOp::StoreOp::Store} },
		rapi::RenderPassOp{ rapi::RenderPassOp::LoadOp::Load, rapi::RenderPassOp::StoreOp::Store },
		rapi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rapi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));
	
	auto [bboxBufferDI, bboxCount] = GetAABBOXBuffer(scene.get(), commandList);

	interop::DebugStage shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferDI();
	shaderConstants.aaboxDI = bboxBufferDI;	
	commandList->PushConstants(shaderConstants);

	commandList->DrawInstanced(24, bboxCount);

	commandList->EndRenderPass();

	return true;
};

void st::gfx::DebugRenderStage::OnAttached()
{
	// Create Framebuffer
	{
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor",
			rapi::ResourceState::RENDERTARGET, rapi::ResourceState::RENDERTARGET);
		m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Read, "SceneDepth",
			rapi::ResourceState::DEPTHSTENCIL, rapi::ResourceState::DEPTHSTENCIL);

		auto fbDesc = rapi::FramebufferDesc()
			.AddColorAttachment(m_RenderView->GetTexture("SceneColor").get())
			.SetDepthAttachment(m_RenderView->GetTexture("SceneDepth").get())
			.SetDebugName("DebugRenderStage");
		m_FB = m_RenderView->GetDeviceManager()->GetDevice()->CreateFramebuffer(fbDesc);
	}

	// Load shaders
	{
		m_VS = m_RenderView->GetDeviceManager()->GetShaderFactory()->LoadShader(
			"AABOX_vs", rapi::ShaderType::Vertex);
		m_PS = m_RenderView->GetDeviceManager()->GetShaderFactory()->LoadShader(
			"AABOX_ps", rapi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rapi::GraphicsPipelineStateDesc desc{
			.VS = m_VS,
			.PS = m_PS,
			.blendState {},
			.depthStencilState = {
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthFunc = rapi::ComparisonFunc::Greater },
			.rasterState = {},
			.primTopo = rapi::PrimitiveTopology::LineList,
			.debugName = "DebugRenderStage"
		};

		m_PSO = m_RenderView->GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
			desc, m_FB->GetFramebufferInfo());
	}
}

void st::gfx::DebugRenderStage::OnDetached()
{
	assert(0);
}

std::pair<st::rapi::DescriptorIndex, size_t> st::gfx::DebugRenderStage::GetAABBOXBuffer(const Scene* scene, rapi::CommandListHandle commandList)
{
	rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	// Check the number of aabox we need
	if (!scene)
		return { rapi::c_InvalidDescriptorIndex, 0 };

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
		return { rapi::c_InvalidDescriptorIndex, 0 };

	if (!m_AABBOXBuffer || (m_AABBOXBuffer->GetDesc().sizeBytes < (aabboxes.size() * sizeof(interop::AABB))))
	{
		if (m_AABBOXBuffer)
		{
			device->ReleaseQueued(m_AABBOXBuffer);
		}

		m_AABBOXBuffer = device->CreateBuffer(
			rapi::BufferDesc{
				.memoryAccess = rapi::MemoryAccess::Upload,
				.shaderUsage = rapi::BufferShaderUsage::ShaderResource,
				.sizeBytes = aabboxes.size() * sizeof(interop::AABB),
				.stride = sizeof(interop::AABB),
				.debugName = "AABBOX buffer" },
			rapi::ResourceState::SHADER_RESOURCE);
	}

	void *ptr = m_AABBOXBuffer->Map();
	std::memcpy(ptr, aabboxes.data(), aabboxes.size() * sizeof(interop::AABB));
	m_AABBOXBuffer->Unmap();

	return { m_AABBOXBuffer->GetShaderViewIndex(rapi::BufferShaderView::ShaderResource), aabboxes.size() };
}