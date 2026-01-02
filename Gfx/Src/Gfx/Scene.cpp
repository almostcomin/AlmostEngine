#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Material.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"
#include "Core/unique_vector.h"
#include <cassert>

st::gfx::Scene::Scene(DeviceManager* deviceManager) : m_DeviceManager{ deviceManager }
{}

st::gfx::Scene::~Scene()
{
	m_DeviceManager->GetDevice()->ReleaseQueued(m_MaterialsBuffer);
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

	auto* dataUploader = m_DeviceManager->GetDataUploader();

	// Each MeshInstance has a, unique transform and a index to a shared mesh
	if (!meshInstances.empty())
	{
		// Fill instances buffer
		unique_vector<st::gfx::Mesh*> meshes;
		{
			rhi::BufferDesc desc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::ShaderResource,
				.sizeBytes = meshInstances.size() * sizeof(interop::InstanceData),
				.allowUAV = false,
				.format = rhi::Format::UNKNOWN,
				.stride = sizeof(interop::InstanceData) };

			m_InstancesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Instances Buffer");

			auto uploadTicket = dataUploader->RequestUploadTicket(desc);
			auto* instanceDataPtr = (interop::InstanceData*)uploadTicket->GetPtr();
			for (const auto* meshInstance : meshInstances)
			{
				instanceDataPtr->modelMatrix = meshInstance->GetNode()->GetWorldTransform();
				instanceDataPtr->inverseModelMatrix = glm::inverse(instanceDataPtr->modelMatrix);
				instanceDataPtr->meshIndex = meshes.insert(meshInstance->GetMesh().get());

				instanceDataPtr++;
			}
			auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_InstancesBuffer.get_weak(),
				rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
			uploadResult->Wait();
		}

		// Fill meshes buffer
		unique_vector<st::gfx::Material*> materials;
		{
			assert(!meshes.empty());
			rhi::BufferDesc desc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::ShaderResource,
				.sizeBytes = meshes.size() * sizeof(interop::MeshData),
				.allowUAV = false,
				.format = rhi::Format::UNKNOWN,
				.stride = sizeof(interop::MeshData) };

			m_MeshesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Meshes Buffer");

			auto uploadTicket = dataUploader->RequestUploadTicket(desc);
			auto* meshDataPtr = (interop::MeshData*)uploadTicket->GetPtr();
			for (const st::gfx::Mesh* mesh : meshes)
			{
				meshDataPtr->indexBufferDI = mesh->GetIndexBuffer()->GetShaderViewIndex(rhi::BufferShaderView::ShaderResource);
				meshDataPtr->indexOffset = 0;
				meshDataPtr->vertexBufferDI = mesh->GetVertexBuffer()->GetShaderViewIndex(rhi::BufferShaderView::ShaderResource);
				meshDataPtr->vertexBufferOffsetBytes = 0;
				const auto& vertexFormat = mesh->GetVertexFormat();
				meshDataPtr->vertexStride = vertexFormat.VertexStride;
				meshDataPtr->vertexPositionOffset = vertexFormat.PositionOffset;
				meshDataPtr->vertexNormalOffset = vertexFormat.NormalOffset;
				meshDataPtr->vertexTangetOffset = vertexFormat.TangentOffset;
				meshDataPtr->vertexTexCoord0Offset = vertexFormat.TexCoord0Offset;
				meshDataPtr->vertexTexCoord1Offset = vertexFormat.TexCoord1Offset;
				meshDataPtr->vertexColorOffset = vertexFormat.ColorOffset;
				meshDataPtr->materialIdx = mesh->GetMaterial() ? materials.insert(mesh->GetMaterial().get()) : rhi::c_InvalidDescriptorIndex;

				meshDataPtr++;
			}
			auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MeshesBuffer.get_weak(),
				rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
			uploadResult->Wait();
		}

		// Materials buffer
		if(!materials.empty())
		{
			rhi::BufferDesc desc{
				.memoryAccess = rhi::MemoryAccess::Default,
				.shaderUsage = rhi::BufferShaderUsage::ShaderResource,
				.sizeBytes = materials.size() * sizeof(interop::MaterialData),
				.allowUAV = false,
				.format = rhi::Format::UNKNOWN,
				.stride = sizeof(interop::MaterialData) };

			m_MaterialsBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Materials Buffer");
			
			auto uploadTicket = dataUploader->RequestUploadTicket(desc);
			auto* matDataPtr = (interop::MaterialData*)uploadTicket->GetPtr();
			for (const st::gfx::Material* mat : materials)
			{
				matDataPtr->baseColorTextureDI = mat->GetBaseColorTextureHandle() ? 
					mat->GetBaseColorTextureHandle()->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource) : rhi::c_InvalidDescriptorIndex;
				matDataPtr->metalRoughTextureDI = mat->GetMetalRoughTextureHandle() ?
					mat->GetMetalRoughTextureHandle()->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource) : rhi::c_InvalidDescriptorIndex;
				matDataPtr->baseColor = { mat->GetBaseColor().x, mat->GetBaseColor().y, mat->GetBaseColor().z, mat->GetOpacity() };
				matDataPtr->mr = { mat->GetMetallicFactor(), mat->GetRoughnessFactor() };

				matDataPtr++;
			}
			auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MaterialsBuffer.get_weak(),
				rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
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

st::rhi::DescriptorIndex st::gfx::Scene::GetInstancesBufferDI() const
{
	return m_InstancesBuffer ? m_InstancesBuffer->GetShaderViewIndex(st::rhi::BufferShaderView::ShaderResource) : rhi::c_InvalidDescriptorIndex;
}

st::rhi::DescriptorIndex st::gfx::Scene::GetMeshesBufferDI() const
{
	return m_MeshesBuffer ? m_MeshesBuffer->GetShaderViewIndex(st::rhi::BufferShaderView::ShaderResource) : rhi::c_InvalidDescriptorIndex;
}

st::rhi::DescriptorIndex st::gfx::Scene::GetMaterialsBufferDI() const
{
	return m_MaterialsBuffer ? m_MaterialsBuffer->GetShaderViewIndex(st::rhi::BufferShaderView::ShaderResource) : rhi::c_InvalidDescriptorIndex;
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