#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStages/ObjectOutlineRenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

void alm::gfx::ObjectOutlineRenderStage::Setup(RenderGraphBuilder& builder)
{
	m_ToneMappedTexture = builder.GetTextureHandle("ToneMapped");
	m_SceneDepthTexture = builder.GetTextureHandle("SceneDepth");

	m_FB = builder.RequestFramebuffer({ m_ToneMappedTexture }, m_SceneDepthTexture);

	builder.AddTextureDependency(m_ToneMappedTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);
	builder.AddTextureDependency(m_SceneDepthTexture, RenderGraph::AccessMode::Write,
		rhi::ResourceState::DEPTHSTENCIL, rhi::ResourceState::DEPTHSTENCIL);
}

void alm::gfx::ObjectOutlineRenderStage::Render(alm::rhi::CommandListHandle commandList)
{
	if (!m_SelectedObject)
		return;
	if (m_SelectedObject.expired())
	{
		m_SelectedObject = nullptr;
		return;
	}
	if (m_SelectedObject->GetLeafSceneIndex() == UINT32_MAX || m_SelectedObject->GetMeshSceneIndex() == UINT32_MAX)
		return;

	interop::ObjectOutlineConstants shaderConstants;
	shaderConstants.SceneDI = GetRenderView()->GetSceneBufferUniformView();
	shaderConstants.InstanceIndex = m_SelectedObject->GetLeafSceneIndex();
	shaderConstants.MeshIndex = m_SelectedObject->GetMeshSceneIndex();
	shaderConstants.ThicknessPx = 0.f;
	shaderConstants.Color = { 1.f, 1.f, 0.f, 1.f };
	shaderConstants.ViewportHeight = (float)m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo().height;

	commandList->PushGraphicsConstants(0, shaderConstants);

	// Mask pass
	commandList->BeginRenderPass(
		m_RenderGraph->GetFrameBuffer(m_FB).get(), 
		{ rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store } },
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store }, // Depth
		rhi::RenderPassOp{ rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store }, // Stencil
		rhi::RenderPassFlags::None);

	commandList->SetPipelineState(m_MaskPSO.get());
	commandList->SetStencilRef(1);
	commandList->Draw(m_SelectedObject->GetMesh()->GetIndexCount());

	shaderConstants.ThicknessPx = 3.f;
	commandList->PushGraphicsConstants(0, shaderConstants);

	commandList->SetPipelineState(m_HullPSO.get());
	commandList->SetStencilRef(1);

	commandList->Draw(m_SelectedObject->GetMesh()->GetIndexCount());

	commandList->EndRenderPass();
}

void alm::gfx::ObjectOutlineRenderStage::OnAttached()
{
	alm::gfx::DeviceManager* deviceManager = GetDeviceManager();
	rhi::Device* device = deviceManager->GetDevice();
	alm::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();

	// Load shaders
	{
		m_VS = shaderFactory->LoadShader("ObjectOutline_vs", rhi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("ObjectOutline_ps", rhi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rhi::RasterizerState rasterState =
		{
			.cullMode = rhi::CullMode::Back
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilEnable = true,
			.stencilReadMask = 0xff,
			.stencilWriteMask = 0xff,
			.frontFaceStencil = {.passOp = rhi::StencilOp::Replace, .func = rhi::ComparisonFunc::Always },
		};

		m_MaskPSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_VS.get_weak(),
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_MaskPSO = device->CreateGraphicsPipelineState(
			m_MaskPSODesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "MaskPSO");

		depthStencilState.frontFaceStencil =
			rhi::DepthStencilOp{ .passOp = rhi::StencilOp::Keep, .func = rhi::ComparisonFunc::NotEqual };

		m_HullPSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_VS.get_weak(),
			.PS = m_PS.get_weak(),
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_HullPSO = device->CreateGraphicsPipelineState(
			m_HullPSODesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "HullPSO");
	}
}

void alm::gfx::ObjectOutlineRenderStage::OnDetached()
{
	m_HullPSO.reset();
	m_MaskPSO.reset();
	m_PS.reset();
	m_VS.reset();
}

void alm::gfx::ObjectOutlineRenderStage::OnBackbufferResize()
{
	rhi::Device* device = GetDeviceManager()->GetDevice();

	m_MaskPSO = device->CreateGraphicsPipelineState(
		m_MaskPSODesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "MaskPSO");
	m_HullPSO = device->CreateGraphicsPipelineState(
		m_HullPSODesc, m_RenderGraph->GetFrameBuffer(m_FB)->GetFramebufferInfo(), "HullPSO");
}