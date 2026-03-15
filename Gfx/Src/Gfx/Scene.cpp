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

st::gfx::Scene::Scene(DeviceManager* deviceManager) : m_DeviceManager{ deviceManager }
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

st::gfx::Scene::~Scene()
{
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MaterialsBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MeshesBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_InstancesBuffer));

	m_SceneGraph.reset();
}

void st::gfx::Scene::SetSceneGraph(unique<SceneGraph>&& graph)
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();

	// Release old data
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MaterialsBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_MeshesBuffer));
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_InstancesBuffer));

	m_SceneGraph = std::move(graph);
	if (!m_SceneGraph)
		return;

	m_SceneGraph->Refresh(); // Make sure it is up to date

	// Fill instances buffer
	if (!m_SceneGraph->GetMeshInstances().empty())
	{
		const std::vector<st::gfx::MeshInstance*> meshInstances = m_SceneGraph->GetMeshInstances();

		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = meshInstances.size() * sizeof(interop::InstanceData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::InstanceData) };

		m_InstancesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Instances Buffer");

		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* instanceDataPtr = (interop::InstanceData*)uploadTicket->GetPtr();
		for (const auto* meshInstance : meshInstances)
		{
			instanceDataPtr->modelMatrix = meshInstance->GetNode()->GetWorldTransform();
			instanceDataPtr->inverseModelMatrix = glm::inverse(instanceDataPtr->modelMatrix);
			instanceDataPtr->meshIndex = meshInstance->GetMeshSceneIndex();

			instanceDataPtr++;
		}
		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_InstancesBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	}

	// Fill meshes buffer
	if(!m_SceneGraph->GetMeshes().empty())
	{
		const auto& meshes = m_SceneGraph->GetMeshes();

		assert(!meshes.empty());
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = meshes.size() * sizeof(interop::MeshData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::MeshData) };

		m_MeshesBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Meshes Buffer");

		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* meshDataPtr = (interop::MeshData*)uploadTicket->GetPtr();
		for (const st::gfx::Mesh* mesh : meshes)
		{
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

			meshDataPtr++;
		}

		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MeshesBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	} // meshes buffer

	// Materials buffer
	if(!m_SceneGraph->GetMaterials().empty())
	{
		const auto& materials = m_SceneGraph->GetMaterials();

		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Default,
			.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
			.sizeBytes = materials.size() * sizeof(interop::MaterialData),
			.format = rhi::Format::UNKNOWN,
			.stride = sizeof(interop::MaterialData) };

		m_MaterialsBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COPY_DST, "Scene Materials Buffer");
			
		auto uploadTicket = dataUploader->RequestUploadTicket(desc);
		auto* matDataPtr = (interop::MaterialData*)uploadTicket->GetPtr();
		for (const st::gfx::Material* mat : materials)
		{
			*matDataPtr = {};
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

			matDataPtr++;
		}
		auto uploadResult = dataUploader->CommitUploadBufferTicket(std::move(*uploadTicket), m_MaterialsBuffer.get_weak(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE);
		uploadResult->Wait();
	} // material buffer
}

void st::gfx::Scene::Update()
{
	if (m_SceneGraph)
	{
		m_SceneGraph->Refresh();
	}
}

const st::math::aabox3f st::gfx::Scene::GetWorldBounds(BoundsType boundsType) const
{
	if (m_SceneGraph && m_SceneGraph->GetRoot() && m_SceneGraph->GetRoot()->HasBounds(boundsType))
	{
		return m_SceneGraph->GetRoot()->GetWorldBounds(boundsType);
	}
	return math::aabox3f::get_empty();
}

st::rhi::BufferReadOnlyView st::gfx::Scene::GetInstancesBufferView() const
{
	return m_InstancesBuffer ? m_InstancesBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

st::rhi::BufferReadOnlyView st::gfx::Scene::GetMeshesBufferView() const
{
	return m_MeshesBuffer ? m_MeshesBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

st::rhi::BufferReadOnlyView st::gfx::Scene::GetMaterialsBufferView() const
{
	return m_MaterialsBuffer ? m_MaterialsBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}
