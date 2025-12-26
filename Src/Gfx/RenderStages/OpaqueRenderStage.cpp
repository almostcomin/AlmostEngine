#include "Gfx/RenderStages/OpaqueRenderStage.h"
#include "Core/Log.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "RenderAPI/Device.h"
#include "Interop/RenderResources.h"

bool st::gfx::OpaqueRenderStage::Render()
{
	auto scene = m_RenderView->GetScene();
	if (!scene)
	{
		//LOG_WARNING("No scene set. Nothing to render");
		return false;
	}

	auto camera = m_RenderView->GetCamera();
	if (!camera)
	{
		LOG_WARNING("No camera set. Can't render without a camera.");
		return false;
	}

	rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();
	auto commandList = m_RenderView->GetCommandList();

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rapi::RenderPassOp{rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::ColorBlack()} },
		rapi::RenderPassOp{ rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::DepthZero() },
		rapi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	commandList->SetViewport(rapi::ViewportState().AddViewportAndScissorRect({
		(float)m_FB->GetFramebufferInfo().width, (float)m_FB->GetFramebufferInfo().height }));

	interop::OpaqueStage shaderConstants;
	shaderConstants.sceneDI = m_RenderView->GetSceneBufferDI();
	shaderConstants.instanceBufferDI = scene->GetInstancesBufferDI();
	shaderConstants.meshesBufferDI = scene->GetMeshesBufferDI();

	const auto& frustum = camera->GetFrustum();
	st::gfx::SceneGraph::Walker walker{ *scene->GetSceneGraph() };
	while(walker)
	{
		auto node = *walker;
		if (hasFlag(node->GetContentFlags(), SceneContentFlags::OpaqueMeshes) && node->HasBounds() && frustum.check(node->GetWorldBounds()))
		{
			auto leaf = node->GetLeaf();
			if (leaf)
			{
				auto* meshInstance = st::checked_cast<st::gfx::MeshInstance*>(leaf.get());
				shaderConstants.instanceIdx = scene->GetInstanceIndex(meshInstance);

				commandList->PushConstants(shaderConstants);
				commandList->Draw(meshInstance->GetMesh()->GetIndexCount());
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	} // while(walker)

	commandList->EndRenderPass();

	return true;
}

void st::gfx::OpaqueRenderStage::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rapi::Device* device = deviceManager->GetDevice();

	auto fbInfo = m_RenderView->GetFramebuffer()->GetFramebufferInfo();

	// Create render targets
	m_RenderView->CreateColorTarget("SceneColor", RenderView::c_BBSize, RenderView::c_BBSize, rapi::Format::SRGBA8_UNORM);
	m_RenderTarget = m_RenderView->GetTexture("SceneColor");
	m_RenderView->CreateDepthTarget("SceneDepth", RenderView::c_BBSize, RenderView::c_BBSize, rapi::Format::D24S8);
	m_DepthStencil = m_RenderView->GetTexture("SceneDepth");

	// Request RT access
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneColor", rapi::ResourceState::RENDERTARGET, rapi::ResourceState::RENDERTARGET);
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "SceneDepth", rapi::ResourceState::DEPTHSTENCIL, rapi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	{
		auto fbDesc = rapi::FramebufferDesc()
			.AddColorAttachment(m_RenderTarget.get())
			.SetDepthAttachment(m_DepthStencil.get())
			.SetDebugName("OpaqueRenderStage");
		m_FB = device->CreateFramebuffer(fbDesc);
	}

	// Load shaders
	{
		st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
		m_VS = shaderFactory->LoadShader("OpaqueStage_vs.vso", rapi::ShaderType::Vertex);
		m_PS = shaderFactory->LoadShader("OpaqueStage_ps.vso", rapi::ShaderType::Pixel);
	}

	// Create PSO
	{
		rapi::BlendState blendState;
		blendState.renderTarget[0] = rapi::BlendState::RenderTargetBlendState
		{
			.blendEnable = false,
		};

		rapi::RasterizerState rasterState =
		{
			.fillMode = rapi::FillMode::Solid,
			.cullMode = rapi::CullMode::Back
		};

		rapi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthFunc = rapi::ComparisonFunc::GreaterEqual,
			.stencilEnable = false
		};

		auto PSODesc = rapi::GraphicsPipelineStateDesc
		{
			.VS = m_VS,
			.PS = m_PS,
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState
		};

		m_PSO = device->CreateGraphicsPipelineState(
			rapi::GraphicsPipelineStateDesc{
				.VS = m_VS,
				.PS = m_PS,
				.blendState = blendState,
				.depthStencilState = depthStencilState,
				.rasterState = rasterState,
				.debugName = "OpaqueRenderStage" },
				m_FB->GetFramebufferInfo());
	}
}

void st::gfx::OpaqueRenderStage::OnDetached()
{
	st::rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_FB);

	m_RenderTarget = nullptr;
	m_DepthStencil = nullptr;

	device->ReleaseQueued(m_PSO);
	device->ReleaseQueued(m_VS);
	device->ReleaseQueued(m_PS);
}