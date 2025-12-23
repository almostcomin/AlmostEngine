#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Material.h"
#include "RenderAPI/Device.h"
#include "Interop/RenderResources.h"
#include "Core/unique_vector.h"
#include <cassert>

st::gfx::Scene::Scene(DeviceManager* deviceManager) : m_DeviceManager{ deviceManager }
{}

st::gfx::Scene::~Scene()
{
	m_DeviceManager->GetDevice()->ReleaseQueued(m_MeshesBuffer);
	m_DeviceManager->GetDevice()->ReleaseQueued(m_InstancesBuffer);

	m_SceneGraph.reset();
}

void st::gfx::Scene::SetSceneGraph(unique<SceneGraph>&& graph)
{
	m_SceneGraph = std::move(graph);
	m_SceneGraph->Refresh(); // Make sure it is up to date

	std::vector<const st::gfx::MeshInstance*> meshInstances;

	SceneGraph::Walker walker{ *m_SceneGraph };
	while (walker)
	{
		if (any(walker->GetContentFlags() & (SceneContentFlags::OpaqueMeshes | SceneContentFlags::AlphaTestedMeshes)))
		{
			auto leaf = walker->GetLeaf();
			if (leaf && any(leaf->GetContentFlags() & (SceneContentFlags::OpaqueMeshes | SceneContentFlags::AlphaTestedMeshes)))
			{
				auto* meshInstancePtr = checked_cast<st::gfx::MeshInstance*>(leaf.get());
				m_InstanceIndices.insert({ meshInstancePtr, meshInstances.size() });
				meshInstances.push_back(meshInstancePtr);
			}
			
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}

	// Each MeshInstance has a, unique transform and a index to a shared mesh
	if (!meshInstances.empty())
	{
		unique_vector<st::gfx::Mesh*> meshes;

		// Fill instances buffer
		{
			rapi::BufferDesc desc{
				.memoryAccess = rapi::MemoryAccess::Default,
				.shaderUsage = rapi::BufferShaderUsage::ShaderResource,
				.sizeBytes = meshInstances.size() * sizeof(interop::InstanceData),
				.allowUAV = false,
				.format = rapi::Format::UNKNOWN,
				.stride = sizeof(interop::InstanceData),
				.debugName = "Scene InstancesBuffer" };

			m_InstancesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rapi::ResourceState::COPY_DST);

			auto* dataUploader = m_DeviceManager->GetDataUploader();
			auto uploadTicket = dataUploader->RequestUploadTicket(desc);
			auto* instanceDataPtr = (interop::InstanceData*)uploadTicket->GetPtr();
			for (const auto* meshInstance : meshInstances)
			{
				instanceDataPtr->modelMatrix = meshInstance->GetNode()->GetWorldTransform();
				instanceDataPtr->inverseModelMatrix = glm::inverse(instanceDataPtr->modelMatrix);
				instanceDataPtr->meshIndex = meshes.insert(meshInstance->GetMesh().get());

				instanceDataPtr++;
			}
			auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_InstancesBuffer, 
				rapi::ResourceState::COPY_DST, rapi::ResourceState::SHADER_RESOURCE);
			uploadResult->Wait();
		}

		// Fill meshes buffer
		{
			assert(!meshes.empty());
			rapi::BufferDesc desc{
				.memoryAccess = rapi::MemoryAccess::Default,
				.shaderUsage = rapi::BufferShaderUsage::ShaderResource,
				.sizeBytes = meshes.size() * sizeof(interop::MeshData),
				.allowUAV = false,
				.format = rapi::Format::UNKNOWN,
				.stride = sizeof(interop::MeshData),
				.debugName = "Scene MeshesBuffer" };

			m_MeshesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rapi::ResourceState::COPY_DST);

			auto* dataUploader = m_DeviceManager->GetDataUploader();
			auto uploadTicket = dataUploader->RequestUploadTicket(desc);
			auto* meshDataPtr = (interop::MeshData*)uploadTicket->GetPtr();
			for (const st::gfx::Mesh* mesh : meshes)
			{
				meshDataPtr->indexBufferDI = mesh->GetIndexBuffer()->GetShaderViewIndex(rapi::BufferShaderView::ShaderResource);
				meshDataPtr->indexOffset = 0;
				meshDataPtr->vertexBufferDI = mesh->GetVertexBuffer()->GetShaderViewIndex(rapi::BufferShaderView::ShaderResource);
				meshDataPtr->vertexBufferOffsetBytes = 0;
				const auto& stride = mesh->GetVertexStride();
				meshDataPtr->vertexStride = stride.Vertex;
				meshDataPtr->vertexPositionStride = stride.Position;
				meshDataPtr->vertexNormalStride = stride.Normal;
				meshDataPtr->vertexTangetStride = stride.Tangent;
				meshDataPtr->vertexTexCoord0Stride = stride.TexCoord0;
				meshDataPtr->vertexTexCoord1Stride = stride.TexCoord1;
				meshDataPtr->vertexColorStride = stride.Color;
				meshDataPtr->textureDI = mesh->GetMaterial()->GetDiffuseTexture() ?
					mesh->GetMaterial()->GetDiffuseTexture()->GetShaderViewIndex(rapi::TextureShaderView::ShaderResource) : rapi::c_InvalidDescriptorIndex;

				meshDataPtr++;
			}
			auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MeshesBuffer,
				rapi::ResourceState::COPY_DST, rapi::ResourceState::SHADER_RESOURCE);
			uploadResult->Wait();
		}
	}
}

void st::gfx::Scene::Update()
{
	if (m_SceneGraph)
	{
		m_SceneGraph->Refresh();
	}
}

st::rapi::DescriptorIndex st::gfx::Scene::GetInstancesBufferDI() const
{
	return m_InstancesBuffer ? m_InstancesBuffer->GetShaderViewIndex(st::rapi::BufferShaderView::ShaderResource) : rapi::c_InvalidDescriptorIndex;
}

st::rapi::DescriptorIndex st::gfx::Scene::GetMeshesBufferDI() const
{
	return m_MeshesBuffer ? m_MeshesBuffer->GetShaderViewIndex(st::rapi::BufferShaderView::ShaderResource) : rapi::c_InvalidDescriptorIndex;
}

int st::gfx::Scene::GetInstanceIndex(const st::gfx::MeshInstance* pInstance)
{
	auto it = m_InstanceIndices.find(pInstance);
	if (it != m_InstanceIndices.end())
	{
		return it->second;
	}
	else
	{
		return -1;
	}
}