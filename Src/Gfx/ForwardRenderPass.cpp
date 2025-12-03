#include "Gfx/ForwardRenderPass.h"
#include "Core/Log.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/RenderView.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/Camera.h"
#include "Gfx/MeshInstance.h"
#include "RenderAPI/Device.h"

bool st::gfx::ForwardRenderPass::Render()
{ return true;

	if (!m_SceneGraph)
	{
		LOG_WARNING("No scene graph set. Nothing to render");
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

	const auto& frustum = camera->GetFrustum();
	st::gfx::SceneGraph::Walker walker{ *m_SceneGraph };
	while(walker)
	{
		auto node = *walker;
		if (node->HasBounds() && frustum.check(node->GetBounds()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetContentFlags() == SceneContentFlags::OpaqueMeshes)
			{
				const st::math::aabox3f worldBounds = leaf->GetBounds() * node->GetWorldTransform();
				if (frustum.check(worldBounds))
				{
					auto meshInstance = dynamic_cast<st::gfx::MeshInstance*>(leaf.get());
					if (meshInstance)
					{
#if 0
						nvrhi::BindingLayoutDesc layoutDesc;
						layoutDesc.visibility = nvrhi::ShaderType::All;
						layoutDesc.bindings = {
							nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 2),
							nvrhi::BindingLayoutItem::Texture_SRV(0),
							nvrhi::BindingLayoutItem::Sampler(0)
						};
						auto bindingLayout = m_Device->createBindingLayout(layoutDesc);

						// Create PSO
						nvrhi::GraphicsPipelineDesc psoDesc = {};
						psoDesc.primType = nvrhi::PrimitiveType::TriangleList;
						//psoDesc.inputLayout = shaderAttribLayout;
						//psoDesc.VS = vertexShader;
						//psoDesc.PS = pixelShader;
						psoDesc.renderState = {}; // let's use default one
						psoDesc.bindingLayouts 

						// Set up graphics state
						nvrhi::GraphicsState drawState;
						drawState.framebuffer = frameBuffer;
						drawState.pipeline = GetPSO(drawState.framebuffer);

						// TODO
#endif
					}
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

	return true;
}

void st::gfx::ForwardRenderPass::OnAttached()
{
#if 0
	rapi::Device* device = m_RenderView->GetDeviceManager()->GetDevice();

	rapi::CommandListParams params{
		.queueType = rapi::QueueType::Graphics
	};
	m_CommandList = device->CreateCommandList(params);
	
	st::gfx::ShaderFactory* shaderFactory = m_RenderView->GetDeviceManager()->GetShaderFactory();
	m_Vs = shaderFactory->CreateShader("Shaders/forward_vs.vso", st::rapi::ShaderType::Vertex);
	m_Ps = shaderFactory->CreateShader("Shaders/forward_ps.pso", st::rapi::ShaderType::Pixel);
#endif
}

void st::gfx::ForwardRenderPass::OnDetached()
{
	m_Vs.reset();
	m_Ps.reset();
}