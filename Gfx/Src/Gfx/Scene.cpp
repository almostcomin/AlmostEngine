#include "Gfx/GfxPCH.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Material.h"
#include "Gfx/SceneLights.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"
#include "Core/unique_vector.h"
#include <cassert>

alm::gfx::Scene::Scene(DeviceManager* deviceManager) : m_DeviceManager{ deviceManager }
{
	m_AmbientParams = AmbientParams{
		.SkyColor = float3{ 0.17f, 0.37f, 0.65f },
		.GroundColor = float3{ 0.62f, 0.58f, 0.55f },
		.Intensity = 0.22f
	};

	m_SunParams = SunParams{
		.ElevationDeg = 60.f,
		.AzimuthDeg = -135.f,
		.Irradiance = 1.f,
		.AngularSizeDeg = 0.53f,
		.Color = float3{ 1.f, 1.f, 1.f },
	};
}

alm::gfx::Scene::~Scene()
{
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MaterialsBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MeshesBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_InstancesBuffer));

	m_SceneGraph.reset();
}

void alm::gfx::Scene::SetSceneGraph(unique<SceneGraph>&& graph)
{
	m_SceneGraph = std::move(graph);
	if (!m_SceneGraph)
		return;

	RefreshSceneGraph();
}

void alm::gfx::Scene::RefreshSceneGraph()
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();

	// Release old data
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MaterialsBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MeshesBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_InstancesBuffer));

	m_SceneGraph->Refresh(); // Make sure it is up to date

	// Fill instances buffer
	if (!m_SceneGraph->GetMeshInstances().empty())
	{
		const auto& meshInstances = m_SceneGraph->GetMeshInstances();

		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = meshInstances.storage_size() * sizeof(interop::InstanceData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::InstanceData) };

		m_InstancesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Instances Buffer");

		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* instanceDataPtr = (interop::InstanceData*)uploadTicket->GetPtr();
		for(auto it = meshInstances.begin_all(); it != meshInstances.end_all(); it++)
		{
			*instanceDataPtr = {};
			if (it.valid_index())
			{
				const auto* meshInstance = *it;

				instanceDataPtr->modelMatrix = meshInstance->GetNode()->GetWorldTransform();
				instanceDataPtr->inverseModelMatrix = glm::inverse(instanceDataPtr->modelMatrix);
				instanceDataPtr->meshIndex = meshInstance->GetMeshSceneIndex();
			}
			instanceDataPtr++;
		}
		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_InstancesBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	}

	// Fill meshes buffer
	if (!m_SceneGraph->GetMeshes().empty())
	{
		const auto& meshes = m_SceneGraph->GetMeshes();

		assert(!meshes.empty());
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = meshes.storage_size() * sizeof(interop::MeshData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::MeshData) };

		m_MeshesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Meshes Buffer");

		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* meshDataPtr = (interop::MeshData*)uploadTicket->GetPtr();
		for (auto it = meshes.begin_all(); it != meshes.end_all(); it++)
		{
			*meshDataPtr = {};
			if (it.valid_index())
			{
				const alm::gfx::Mesh* mesh = it->get();

				meshDataPtr->indexBufferDI = mesh->GetIndexBuffer()->GetReadOnlyView();
				meshDataPtr->indexSize = mesh->GetIndexSize();
				meshDataPtr->indexOffsetBytes = 0;
				meshDataPtr->vertexBufferDI = mesh->GetVertexBuffer()->GetReadOnlyView();
				meshDataPtr->vertexBufferOffsetBytes = 0;
				const auto& vertexFormat = mesh->GetVertexFormat();
				meshDataPtr->vertexStride = vertexFormat.VertexStride;
				meshDataPtr->vertexPositionOffset = vertexFormat.PositionOffset;
				meshDataPtr->vertexNormalOffset = vertexFormat.NormalOffset;
				meshDataPtr->vertexTangetOffset = vertexFormat.TangentOffset;
				meshDataPtr->vertexTexCoord0Offset = vertexFormat.TexCoord0Offset;
				meshDataPtr->vertexTexCoord1Offset = vertexFormat.TexCoord1Offset;
				meshDataPtr->vertexColorOffset = vertexFormat.ColorOffset;
				meshDataPtr->materialIdx = m_SceneGraph->GetMaterialIndex(mesh);
			}
			meshDataPtr++;
		}

		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MeshesBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	} // meshes buffer

	// Materials buffer
	if (!m_SceneGraph->GetMaterials().empty())
	{
		const auto& materials = m_SceneGraph->GetMaterials();

		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = materials.storage_size() * sizeof(interop::MaterialData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::MaterialData) };

		m_MaterialsBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Materials Buffer");

		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* matDataPtr = (interop::MaterialData*)uploadTicket->GetPtr();

		for (auto it = materials.begin_all(); it != materials.end_all(); it++)
		{
			*matDataPtr = {};
			if (it.valid_index())
			{
				const alm::gfx::Material* mat = it->get();
				if (mat->GetBaseColorTextureHandle())
				{
					matDataPtr->baseColorTextureDI = mat->GetBaseColorTextureHandle()->GetSampledView();
				}
				if (mat->GetEmissiveTextureHandle())
				{
					matDataPtr->emissiveTextureDI = mat->GetEmissiveTextureHandle()->GetSampledView();
				}
				if (mat->GetMetalRoughTextureHandle())
				{
					matDataPtr->metalRoughTextureDI = mat->GetMetalRoughTextureHandle()->GetSampledView();
				}
				if (mat->GetNormalTextureHandle())
				{
					matDataPtr->normalTextureDI = mat->GetNormalTextureHandle()->GetSampledView();
				}
				if (mat->GetOcclusionTextureHandle())
				{
					matDataPtr->occlusionTextureDI = mat->GetOcclusionTextureHandle()->GetSampledView();
				}

				matDataPtr->normalScale = mat->GetNormalTextureScale();
				matDataPtr->baseColor = { mat->GetBaseColor().x, mat->GetBaseColor().y, mat->GetBaseColor().z, mat->GetOpacity() };
				matDataPtr->emissiveColor = mat->GetEmissiveColor();
				matDataPtr->occlusion = mat->GetOcclusionStrengh();
				matDataPtr->metalness = mat->GetMetallicFactor();
				matDataPtr->roughness = mat->GetRoughnessFactor();
				matDataPtr->alphaCutoff = mat->GetAlphaCutoff();
			}
			matDataPtr++;
		}
		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MaterialsBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	} // material buffer
}

void alm::gfx::Scene::Update()
{
	if (m_SceneGraph)
	{
		m_SceneGraph->Refresh();
	}
}

const alm::math::aabox3f alm::gfx::Scene::GetWorldBounds(SceneContentType type) const
{
	if (m_SceneGraph && m_SceneGraph->GetRoot() && has_any_flag(m_SceneGraph->GetRoot()->GetContentFlags(), ToFlag(type)))
	{
		return m_SceneGraph->GetRoot()->GetWorldBounds(type);
	}
	return math::aabox3f::get_empty();
}

alm::rhi::BufferReadOnlyView alm::gfx::Scene::GetInstancesBufferView() const
{
	return m_InstancesBuffer ? m_InstancesBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

alm::rhi::BufferReadOnlyView alm::gfx::Scene::GetMeshesBufferView() const
{
	return m_MeshesBuffer ? m_MeshesBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

alm::rhi::BufferReadOnlyView alm::gfx::Scene::GetMaterialsBufferView() const
{
	return m_MaterialsBuffer ? m_MaterialsBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}
