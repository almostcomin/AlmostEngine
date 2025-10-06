#include "Gfx/ForwardRenderPass.h"
#include "Core/Log.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/RenderView.h"
#include "Gfx/Camera.h"
#include "Gfx/Mesh.h"
#include "Gfx/DeviceManager.h"

bool st::gfx::ForwardRenderPass::Init(nvrhi::DeviceHandle device)
{
	m_Device = device;

	m_CommandList = device->createCommandList();
	
	return true;
}

bool st::gfx::ForwardRenderPass::Render(nvrhi::IFramebuffer* frameBuffer)
{
	if (!m_SceneGraph)
	{
		LOG_ERROR("No scene graph set. Nothing to render");
		return false;
	}

	auto camera = m_RenderView->GetCamera();
	if (!camera)
	{
		LOG_ERROR("No camera set. Can't render without a camera.");
	}

#if 0
	m_CommandList->open();
	m_CommandList->beginMarker("ForwardRenderPass");

	const auto& frustum = camera->GetFrustum();
	st::gfx::SceneGraph::Walker walker{ *m_SceneGraph };
	while(walker)
	{
		if (walker->HasBounds() && frustum.check(walker->GetBounds()))
		{
			auto leaf = walker->GetLeaf();
			if (leaf && leaf->GetContentFlags() == SceneContentFlags::OpaqueMeshes)
			{
				const st::math::aabox3f worldBounds = leaf->GetBounds() * walker->GetWorldTransform();
				if (frustum.check(worldBounds))
				{
					auto meshInstance = dynamic_cast<st::gfx::MeshInstance*>(leaf.get());
					if (meshInstance)
					{
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

	m_CommandList->endMarker();
	m_CommandList->close();
	m_Device->executeCommandList(m_CommandList, nvrhi::CommandQueue::Graphics);
#endif
	return true;
}