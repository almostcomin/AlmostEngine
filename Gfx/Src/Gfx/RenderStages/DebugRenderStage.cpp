#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "RHI/Device.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/Camera.h"
#include "Interop/RenderResources.h"

void alm::gfx::DebugRenderStage::ShowRenderBBoxes(SceneContentType boundsType, bool b)
{ 
	assert(HasBoundsCategory(boundsType));
	m_RenderBBoxes[(int)boundsType] = b; 
}

void alm::gfx::DebugRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_TonemappedTexture = builder.GetTextureHandle("ToneMapped");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");
	m_GBuffer2Texture = builder.GetTextureHandle("GBuffer2");

	m_FB = builder.RequestFramebuffer({ m_TonemappedTexture }, /*m_SceneDepthTexture*/{});

	builder.AddTextureDependency(m_TonemappedTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
	builder.AddTextureDependency(m_GBuffer2Texture, RenderGraph::AccessMode::Read,
		rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void alm::gfx::DebugRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	RenderBBoxes(commandList);
	//RenderS2H(commandList);
};

void alm::gfx::DebugRenderStage::OnAttached()
{
	CreateBBoxPSO();
	CreateS2HPSO();
}

void alm::gfx::DebugRenderStage::OnDetached()
{
	m_BBoxRenderResources = {};
	m_S2HRenderResources = {};
}

void alm::gfx::DebugRenderStage::OnBackbufferResize()
{
	CreateBBoxPSO();
}

std::pair<alm::rhi::BufferReadOnlyView, size_t> alm::gfx::DebugRenderStage::GetAABBOXBuffer(const Scene* scene, SceneContentType boundsType)
{
	// Check the number of aabox we need
	if (!scene)
		return { rhi::c_InvalidDescriptorIndex, 0 };
	 
	// Collect all bboxes
	std::vector<alm::aabox3f> aabboxes;
	alm::gfx::SceneGraph::Walker walker{ *scene->GetSceneGraph() };
	while (walker)
	{
		const auto* node = *walker;
		if (has_any_flag(node->GetContentFlags(), ToFlag(boundsType)))
		{
			aabboxes.push_back(node->GetWorldBounds(boundsType));
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}

	return FillBuffer(aabboxes);
}

std::pair<alm::rhi::BufferReadOnlyView, size_t> alm::gfx::DebugRenderStage::FillBuffer(const std::vector<alm::aabox3f>& aabboxes)
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	if (aabboxes.empty())
		return { rhi::c_InvalidDescriptorIndex, 0 };

	auto buffer = device->CreateBuffer(
		rhi::BufferDesc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = aabboxes.size() * sizeof(interop::AABB),
			.stride = sizeof(interop::AABB) },
			rhi::ResourceState::SHADER_RESOURCE,
			"AABBOX buffer");

	auto* ptr = (interop::AABB*)buffer->Map();
	for (const auto& bbox : aabboxes)
	{
		ptr->min = bbox.min;
		ptr->max = bbox.max;
		++ptr;
	}
	buffer->Unmap();

	return { buffer->GetReadOnlyView(), aabboxes.size() };
}

void alm::gfx::DebugRenderStage::CreateBBoxPSO()
{
	// Load shaders
	{
		if (!m_BBoxRenderResources.VS)
		{
			m_BBoxRenderResources.VS = GetDeviceManager()->GetShaderFactory()->LoadShader(
				"AABOX_vs", rhi::ShaderType::Vertex);
		}
		if (!m_BBoxRenderResources.PS)
		{
			m_BBoxRenderResources.PS = GetDeviceManager()->GetShaderFactory()->LoadShader(
				"AABOX_ps", rhi::ShaderType::Pixel);
		}
	}

	// Create PSO
	{
		rhi::GraphicsPipelineStateDesc desc{
			.VS = m_BBoxRenderResources.VS.get_weak(),
			.PS = m_BBoxRenderResources.PS.get_weak(),
			.blendState {},
			.depthStencilState = {
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthFunc = rhi::ComparisonFunc::Greater },
			.rasterState = {},
			.primTopo = rhi::PrimitiveTopology::LineList
		};

		m_BBoxRenderResources.PSO = GetDeviceManager()->GetDevice()->CreateGraphicsPipelineState(
			desc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "DebugRenderStage");
	}
}

void alm::gfx::DebugRenderStage::CreateS2HPSO()
{
	if (!m_S2HRenderResources.CS)
	{
		m_S2HRenderResources.CS = GetDeviceManager()->GetShaderFactory()->LoadShader(
			"Debug_cs", rhi::ShaderType::Compute);
	}

	// Create PSO
	{
		rhi::ComputePipelineStateDesc desc{
			.CS = m_S2HRenderResources.CS.get_weak()
		};

		m_S2HRenderResources.PSO = GetDeviceManager()->GetDevice()->CreateComputePipelineState(
			desc, "DebugRenderStage_S2HPSO");
	}
}

void alm::gfx::DebugRenderStage::RenderBBoxes(alm::rhi::CommandListHandle commandList)
{
	const auto* scene = GetScene();
	if (!scene)
		return;

	if (!any(m_RenderBBoxes) && !m_RenderHeightmapBBoxes)
		return;

	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(),
		{ rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store} },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store },
		{},
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_BBoxRenderResources.PSO.get());

	for (int i = 0; i < (int)SceneContentType::_Size; ++i)
	{
		if (m_RenderBBoxes[i])
		{
			auto [bboxBufferDI, bboxCount] = GetAABBOXBuffer(scene, (SceneContentType)i);
			if (bboxCount == 0)
				continue;

			interop::DebugStageBBoxes shaderConstants;
			shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
			shaderConstants.aaboxDI = bboxBufferDI;
			commandList->PushGraphicsConstants(0, shaderConstants);

			commandList->DrawInstanced(24, bboxCount, 0);
		}
	}

	if (m_RenderHeightmapBBoxes)
	{
		for (const auto* sceneHeightmap : scene->GetSceneGraph()->GetSceneHeightmaps())
		{
			alm::gfx::HeightmapInstance* heightmapInstance = GetRenderView()->GetHeightmapInstance(sceneHeightmap);
			std::vector<alm::aabox3f> bboxes = heightmapInstance->CollectAABBoxes();

			auto [bboxBufferDI, bboxCount] = FillBuffer(bboxes);
			if (bboxCount == 0)
				continue;

			interop::DebugStageBBoxes shaderConstants;
			shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
			shaderConstants.aaboxDI = bboxBufferDI;
			commandList->PushGraphicsConstants(0, shaderConstants);

			commandList->DrawInstanced(24, bboxCount, 0);
		}
	}

	commandList->EndRenderPass();
}

void alm::gfx::DebugRenderStage::RenderS2H(alm::rhi::CommandListHandle commandList)
{
	const auto* renderView = GetRenderView();
	const auto* camera = GetCamera();

	commandList->SetPipelineState(m_S2HRenderResources.PSO.get());

	auto tex = m_RenderGraph->GetTexture(m_TonemappedTexture);
	const auto& texDesc = tex->GetDesc();
	const auto mouseState = GetRenderView()->GetMouseState();

	commandList->PushBarrier(rhi::Barrier::Texture(tex.get(),
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::UNORDERED_ACCESS));

	interop::DebugStageS2H shaderConstants;
	shaderConstants.sceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.inputTextureDI = tex->GetStorageView();
	shaderConstants.outputTextureDI = tex->GetStorageView();
	shaderConstants.GBuffer2DI = m_RenderGraph->GetTextureSampledView(m_GBuffer2Texture);

	commandList->PushComputeConstants(0, shaderConstants);

	commandList->Dispatch(DivRoundUp(texDesc.width, 8u), DivRoundUp(texDesc.height, 8u), 1);

	commandList->PushBarrier(rhi::Barrier::Texture(tex.get(),
		rhi::ResourceState::UNORDERED_ACCESS, rhi::ResourceState::RENDERTARGET));
}