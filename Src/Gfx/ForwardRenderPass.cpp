#include "Gfx/ForwardRenderPass.h"
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

bool st::gfx::ForwardRenderPass::Render()
{
	if (!m_Scene)
	{
		LOG_WARNING("No scene set. Nothing to render");
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

	commandList->BeginMarker("ForwardRenderPass");

	commandList->BeginRenderPass(
		m_FB.get(),
		{ rapi::RenderPassOp{rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::Black()} },
		rapi::RenderPassOp{ rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::Zero() },
		rapi::RenderPassFlags::None);

	commandList->SetPipelineState(m_PSO.get());

	interop::ForwardRP fwData;
	fwData.sceneDI = GetSceneDI();
	fwData.instanceBufferDI = m_Scene->GetInstancesBufferDI();
	fwData.meshesBufferDI = m_Scene->GetMeshesBufferDI();

	const auto& frustum = camera->GetFrustum();
	st::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while(walker)
	{
		auto node = *walker;
		if (node->HasBounds() && frustum.check(node->GetBounds()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetContentFlags() == SceneContentFlags::OpaqueMeshes)
			{
				const st::math::aabox3f worldBounds = leaf->GetBounds() * node->GetWorldTransform();
				//if (frustum.check(worldBounds))
				{
					auto* meshInstance = st::checked_cast<st::gfx::MeshInstance*>(leaf.get());
					fwData.instanceIdx = m_Scene->GetInstanceIndex(meshInstance);

					commandList->PushConstants(fwData);
					commandList->Draw(meshInstance->GetMesh()->GetPrimitiveCount() * 3);
				}
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	} // while(walker)

	commandList->EndMarker();

	commandList->EndRenderPass();

	return true;
}

void st::gfx::ForwardRenderPass::OnAttached()
{
	st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
	rapi::Device* device = deviceManager->GetDevice();

	st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();
	m_VS = shaderFactory->LoadShader("ForwardRP_vs.vso", rapi::ShaderType::Vertex);
	m_PS = shaderFactory->LoadShader("ForwardRP_ps.vso", rapi::ShaderType::Pixel);

	auto fbInfo = m_RenderView->GetFramebuffer()->GetFramebufferInfo();

	// Create render targets
	rapi::TextureDesc rtDesc{
		.width = fbInfo.width,
		.height = fbInfo.height,
		.format = rapi::Format::SRGBA8_UNORM,
		.shaderUsage = rapi::TextureShaderUsage::ShaderResource | rapi::TextureShaderUsage::RenderTarget,
		.debugName = "ForwardRenderPass_RT"};	

	m_RenderView->CreateTexture(rtDesc, "ForwardRenderPass_RT");
	m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ForwardRenderPass_RT", rapi::ResourceState::RENDERTARGET, rapi::ResourceState::RENDERTARGET);
	m_RenderTarget = m_RenderView->GetTexture("ForwardRenderPass_RT");

	rapi::TextureDesc dsDesc{
		.width = fbInfo.width,
		.height = fbInfo.height,
		.format = rapi::Format::D24S8,
		.shaderUsage = rapi::TextureShaderUsage::ShaderResource | rapi::TextureShaderUsage::DepthStencil,
		.debugName = "ForwardRenderPass_DS" };
	m_DepthStencil = device->CreateTexture(dsDesc, rapi::ResourceState::DEPTHSTENCIL);

	// Create Framebuffer
	auto fbDesc = rapi::FramebufferDesc()
		.AddColorAttachment(m_RenderTarget.get())
		.SetDepthAttachment(m_DepthStencil.get())
		.SetDebugName("ForwardRenderPass_FB");
	m_FB = device->CreateFramebuffer(fbDesc);

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
			.depthFunc = rapi::ComparisonFunc::LessEqual,
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
				.rasterState = rasterState },
				m_FB->GetFramebufferInfo());
	}
}

void st::gfx::ForwardRenderPass::OnDetached()
{
	st::rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	device->ReleaseQueued(m_SceneCB);

	device->ReleaseQueued(m_RenderTarget);
	device->ReleaseQueued(m_DepthStencil);

	device->ReleaseQueued(m_PSO);
	device->ReleaseQueued(m_VS);
	device->ReleaseQueued(m_PS);
}

st::rapi::DescriptorIndex st::gfx::ForwardRenderPass::GetSceneDI()
{
	auto camera = m_RenderView->GetCamera();
	if (!camera)
	{
		LOG_ERROR("Not camera defined");
		return {};
	}

	if (!m_SceneCB)
	{
		rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

		rapi::BufferDesc desc{
			.memoryAccess = rapi::MemoryAccess::Upload,
			.shaderUsage = rapi::BufferShaderUsage::ConstantBuffer,
			.sizeBytes = sizeof(interop::Scene),
			.allowUAV = false,
			.stride = 0 };

		m_SceneCB = device->CreateBuffer(desc, rapi::ResourceState::SHADER_RESOURCE);
		
		interop::Scene* sceneData = (interop::Scene*)m_SceneCB->Map();
		sceneData->viewProjectionMatrix = camera->GetViewProjectionMatrix();		
		m_SceneCB->Unmap();
	}

	return m_SceneCB->GetShaderViewIndex(rapi::BufferShaderView::ConstantBuffer);
}