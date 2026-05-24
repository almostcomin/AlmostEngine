#include "Gfx/GfxPCH.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/MeshInstance.h"
#include "RHI/Buffer.h"
#include "RHI/Device.h"

static void SerializeMaterial(const alm::gfx::Material* mat, interop::MaterialData* dest)
{
	*dest = {};

	if (mat->GetBaseColorTextureHandle())
	{
		dest->baseColorTextureDI = mat->GetBaseColorTextureHandle()->GetSampledView();
	}
	if (mat->GetEmissiveTextureHandle())
	{
		dest->emissiveTextureDI = mat->GetEmissiveTextureHandle()->GetSampledView();
	}
	if (mat->GetMetalRoughTextureHandle())
	{
		dest->metalRoughTextureDI = mat->GetMetalRoughTextureHandle()->GetSampledView();
	}
	if (mat->GetNormalTextureHandle())
	{
		dest->normalTextureDI = mat->GetNormalTextureHandle()->GetSampledView();
	}
	if (mat->GetOcclusionTextureHandle())
	{
		dest->occlusionTextureDI = mat->GetOcclusionTextureHandle()->GetSampledView();
	}

	dest->normalScale = mat->GetNormalTextureScale();
	dest->baseColor = { mat->GetBaseColor().x, mat->GetBaseColor().y, mat->GetBaseColor().z, mat->GetOpacity() };
	dest->emissiveColor = mat->GetEmissiveColor();
	dest->occlusion = mat->GetOcclusionStrengh();
	dest->metalness = mat->GetMetallicFactor();
	dest->roughness = mat->GetRoughnessFactor();
	dest->alphaCutoff = mat->GetAlphaCutoff();
}

static void SerializeMesh(const alm::gfx::Mesh* mesh, uint32_t materialIdx, interop::MeshData* dest)
{
	const auto& vertexFormat = mesh->GetVertexFormat();

	dest->indexBufferDI = mesh->GetIndexBuffer()->GetReadOnlyView();
	dest->indexSize = mesh->GetIndexSize();
	dest->indexOffsetBytes = 0;
	dest->vertexBufferDI = mesh->GetVertexBuffer()->GetReadOnlyView();
	dest->vertexBufferOffsetBytes = 0;
	dest->vertexStride = vertexFormat.VertexStride;
	dest->vertexPositionOffset = vertexFormat.PositionOffset;
	dest->vertexNormalOffset = vertexFormat.NormalOffset;
	dest->vertexTangetOffset = vertexFormat.TangentOffset;
	dest->vertexTexCoord0Offset = vertexFormat.TexCoord0Offset;
	dest->vertexTexCoord1Offset = vertexFormat.TexCoord1Offset;
	dest->vertexColorOffset = vertexFormat.ColorOffset;
	dest->materialIdx = materialIdx;
}

static void SerializeMeshInstance(const alm::gfx::MeshInstance* mi, interop::InstanceData* dest)
{
	const auto worldMtx = mi->GetWorldTransform();

	dest->modelMatrix = worldMtx;
	dest->inverseModelMatrix = glm::inverse(worldMtx);
}

alm::gfx::GpuSceneBuffers::GpuSceneBuffers(rhi::Device* device) : m_Device{ device }
{
	m_MaterialsBuffer = CreateBuffer<interop::MaterialData>(kMaterialCount, rhi::ResourceState::SHADER_RESOURCE, "Materials GPU Buffer");
	m_MeshesBuffer = CreateBuffer<interop::MeshData>(kMeshRefCount, rhi::ResourceState::SHADER_RESOURCE, "Meshes GPU Buffer");

	m_MeshMaterialIndices.fill(UINT32_MAX);
}

alm::gfx::GpuSceneBuffers::~GpuSceneBuffers()
{}

alm::gfx::GpuSceneBuffersHandle alm::gfx::GpuSceneBuffers::RequestSceneHandle(const std::string& debugName)
{
	GpuSceneBuffersHandle handle;
	handle.idx = m_SceneStates.insert({});
	m_SceneStates[handle.idx].DebugName = debugName;
	m_SceneStates[handle.idx].MeshInstancesBuffer = CreateBuffer<interop::InstanceData>(kStaticInstanceCount + kTransientInstanceCount,
		rhi::ResourceState::SHADER_RESOURCE, std::format("{}|Instances Buffer", debugName));

	return handle;
}

void alm::gfx::GpuSceneBuffers::ReleaseSceneHandle(GpuSceneBuffersHandle handle)
{
	if (m_SceneStates.valid_index(handle.idx))
	{
		// Remove all instances
		for (MeshInstance* mi : m_SceneStates[handle.idx].MeshInstances)
		{
			// Remove material ref
			if (mi->GetMeshSceneIndex() != UINT32_MAX)
			{
				UnregisterMesh(mi->GetMeshSceneIndex());
				mi->SetMeshSceneIndex(UINT32_MAX);
			}
			mi->SetLeafSceneIndex(UINT32_MAX);
		}

		m_SceneStates.erase(handle.idx);
	}
}

uint32_t alm::gfx::GpuSceneBuffers::RegisterMaterial(const gfx::Material* mat)
{
	if (mat == nullptr)
	{
		LOG_ERROR("Trying to register a null material");
		return UINT32_MAX;
	}

	auto materialIdx = m_Materials.find(mat);
	if (materialIdx == m_Materials.capacity())
	{
		materialIdx = m_Materials.insert({ mat, 1 }).first;
		m_MaterialsState.NewIndices.insert(materialIdx);
		m_MaterialsState.RemovedIndices.fast_erase(materialIdx);  // <-- in case slot was reused
	}
	else
	{
		++m_Materials[materialIdx].refCount;
	}

	return materialIdx;
}

uint32_t alm::gfx::GpuSceneBuffers::RegisterMesh(const gfx::Mesh* mesh)
{
	if (mesh == nullptr)
	{
		LOG_ERROR("Trying to register a null mesh");
		return UINT32_MAX;
	}

	auto meshRefIdx = m_Meshes.find(mesh);
	if (meshRefIdx == m_Meshes.capacity())
	{
		meshRefIdx = m_Meshes.insert({ mesh, 1 }).first;
		m_MeshesState.NewIndices.insert(meshRefIdx);
		m_MeshesState.RemovedIndices.fast_erase(meshRefIdx);  // <-- in case slot was reused

		// Add material also so we can keep a list of materials used by meshes
		m_MeshMaterialIndices[meshRefIdx] = RegisterMaterial(mesh->GetMaterial().get());
	}
	else
	{
		++m_Meshes[meshRefIdx].refCount;
	}

	return meshRefIdx;
}

uint32_t alm::gfx::GpuSceneBuffers::RegisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi)
{
	assert(mi);
	assert(mi->GetMesh());
	assert(m_SceneStates.valid_index(handle.idx));

	uint32_t idx = m_SceneStates[handle.idx].MeshInstances.insert(mi);
	m_SceneStates[handle.idx].MeshInstancesState.NewIndices.insert(idx);
	m_SceneStates[handle.idx].MeshInstancesState.RemovedIndices.fast_erase(idx);

	// Add mesh
	mi->SetMeshSceneIndex(RegisterMesh(mi->GetMesh().get()));

	mi->SetLeafSceneIndex(idx);
	return idx;
}

void alm::gfx::GpuSceneBuffers::UnregisterMaterial(const gfx::Material* mat)
{
	UnregisterMaterial(m_Materials.find(mat));
}

void alm::gfx::GpuSceneBuffers::UnregisterMesh(const gfx::Mesh* mesh)
{
	UnregisterMesh(m_Meshes.find(mesh));
}

void alm::gfx::GpuSceneBuffers::UnregisterMaterial(uint32_t idx)
{
	if (m_Materials.valid_index(idx))
	{
		--m_Materials[idx].refCount;
		if (m_Materials[idx].refCount == 0)
		{
			m_Materials.erase(idx);
			m_MaterialsState.RemovedIndices.insert(idx);
			
			m_MaterialsState.DirtyIndices.fast_erase(idx);
			m_MaterialsState.NewIndices.fast_erase(idx);
		}
	}
	else
	{
		LOG_WARNING("Trying to unregister a never registered material with index [{}]", idx);
	}
}

void alm::gfx::GpuSceneBuffers::UnregisterMesh(uint32_t idx)
{
	if (m_Meshes.valid_index(idx))
	{
		--m_Meshes[idx].refCount;
		if (m_Meshes[idx].refCount == 0)
		{
			// Remove material ref
			if (m_MeshMaterialIndices[idx] != UINT32_MAX)
			{
				UnregisterMaterial(m_MeshMaterialIndices[idx]);
				// Remove material binding
				m_MeshMaterialIndices[idx] = UINT32_MAX;
			}

			m_Meshes.erase(idx);
			m_MeshesState.RemovedIndices.insert(idx);

			m_MeshesState.DirtyIndices.fast_erase(idx);
			m_MeshesState.NewIndices.fast_erase(idx);
		}
	}
	else
	{
		LOG_WARNING("Trying to unregister a never registered mesh with index [{}]", idx);
	}
}

void alm::gfx::GpuSceneBuffers::UnregisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi)
{
	assert(mi);
	assert(m_SceneStates.valid_index(handle.idx));
	assert(m_SceneStates[handle.idx].MeshInstances.valid_index(mi->GetLeafSceneIndex()));

	// Remove material ref
	if (mi->GetMeshSceneIndex() != UINT32_MAX)
	{
		UnregisterMesh(mi->GetMeshSceneIndex());
		mi->SetMeshSceneIndex(UINT32_MAX);
	}

	uint32_t idx = mi->GetLeafSceneIndex();
	m_SceneStates[handle.idx].MeshInstances.erase(idx);
	m_SceneStates[handle.idx].MeshInstancesState.RemovedIndices.insert(idx);
	m_SceneStates[handle.idx].MeshInstancesState.DirtyIndices.fast_erase(idx);
	m_SceneStates[handle.idx].MeshInstancesState.NewIndices.fast_erase(idx);

	mi->SetLeafSceneIndex(UINT32_MAX);
}

void alm::gfx::GpuSceneBuffers::SetDirtyMaterial(const gfx::Material* mat)
{
	SetDirtyMaterial(m_Materials.find(mat));
}

void alm::gfx::GpuSceneBuffers::SetDirtyMesh(const gfx::Mesh* mesh)
{
	SetDirtyMesh(m_Meshes.find(mesh));
}

void alm::gfx::GpuSceneBuffers::SetDirtyMaterial(uint32_t idx)
{
	if (m_Materials.valid_index(idx))
	{
		m_MaterialsState.DirtyIndices.insert(idx);
	}
}

void alm::gfx::GpuSceneBuffers::SetDirtyMesh(uint32_t idx)
{
	if (m_Meshes.valid_index(idx))
	{
		m_MeshesState.DirtyIndices.insert(idx);
	}
}

void alm::gfx::GpuSceneBuffers::SetDirtyMeshInstance(GpuSceneBuffersHandle handle, const gfx::MeshInstance* mi)
{
	assert(mi);
	assert(m_SceneStates.valid_index(handle.idx));
	assert(m_SceneStates[handle.idx].MeshInstances.valid_index(mi->GetLeafSceneIndex()));

	m_SceneStates[handle.idx].MeshInstancesState.DirtyIndices.insert(mi->GetLeafSceneIndex());
}

void alm::gfx::GpuSceneBuffers::RebindMeshMaterial(const Mesh* mesh)
{
	auto meshIdx = m_Meshes.find(mesh);
	if (!m_Meshes.valid_index(meshIdx))
		return;

	if (m_MeshMaterialIndices[meshIdx] != UINT32_MAX)
	{
		UnregisterMaterial(m_MeshMaterialIndices[meshIdx]);
	}
	
	const auto& newMat = mesh->GetMaterial();
	m_MeshMaterialIndices[meshIdx] = newMat ? RegisterMaterial(mesh->GetMaterial().get()) : UINT32_MAX;

	SetDirtyMesh(meshIdx);
}

alm::gfx::GpuSceneBuffers::HeightmapPatchesAllocation alm::gfx::GpuSceneBuffers::AllocateTransientHeightmapPatches(
	GpuSceneBuffersHandle handle, uint32_t count)
{
	assert(count > 0);
	assert(m_SceneStates.valid_index(handle.idx));

	auto& ss = m_SceneStates[handle.idx];

	auto [instancesBaseIdx, instanceDataPtr] = AllocateTransientInstances(handle, count);
	if (instanceDataPtr == nullptr)
	{
		LOG_ERROR("Failed allocating '{}' transient instances", count);
		return {};
	}

	// Actually if AllocateTransientInstances succeeded, this should also always succeed.
	// But lets do check anyway, in case we decouple the buffers capacity in the future.
	if (ss.HeighmapPatchesAllocated + count > kTransientInstanceCount)
	{
		LOG_ERROR("Not enough space in the HeightmapPatches buffer.");
		return {};
	}

	if (!ss.HeightmapPatchDataBuffer)
	{
		ss.HeightmapPatchDataBuffer = CreateBuffer<interop::HeightmapPatchData>(
			kTransientInstanceCount, rhi::ResourceState::SHADER_RESOURCE, "HeightmapData");
	}

	if (!ss.HeightmapPatchDataStagingBuffer)
	{
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.sizeBytes = kTransientInstanceCount * sizeof(interop::HeightmapPatchData) };

		ss.HeightmapPatchDataStagingBuffer =
			m_Device->CreateBuffer(desc, rhi::ResourceState::COPY_SRC, std::format("{}|HeightmapPatchData staging", ss.DebugName));

		ss.HeightmapPatchDataPtr = (interop::HeightmapPatchData*)ss.HeightmapPatchDataStagingBuffer->Map();
	}

	HeightmapPatchesAllocation alloc{
		.InstancesBaseIndex = instancesBaseIdx,
		.PatchesBaseIndex = ss.HeighmapPatchesAllocated,
		.Count = count,
		.InstancesDataPtr = instanceDataPtr,
		.HeightmapPatchesPtr = ss.HeightmapPatchDataPtr + ss.HeighmapPatchesAllocated };

	ss.HeighmapPatchesAllocated += count;

	return alloc;
}

uint32_t alm::gfx::GpuSceneBuffers::GetMaterialIndexFromMesh(const alm::gfx::Mesh* mesh) const
{
	return GetMaterialIndexFromMeshIdx(m_Meshes.find(mesh));
}

uint32_t alm::gfx::GpuSceneBuffers::GetMaterialIndexFromMeshIdx(uint32_t meshIdx) const
{
	if (m_Meshes.valid_index(meshIdx))
	{
		return m_MeshMaterialIndices[meshIdx];
	}
	return UINT32_MAX;
}

alm::rhi::BufferReadOnlyView alm::gfx::GpuSceneBuffers::GetMeshesBufferView() const
{
	return m_MeshesBuffer ? m_MeshesBuffer->GetReadOnlyView() : alm::rhi::BufferReadOnlyView{};
}

alm::rhi::BufferReadOnlyView alm::gfx::GpuSceneBuffers::GetMaterialsBufferView() const
{
	return m_MaterialsBuffer ? m_MaterialsBuffer->GetReadOnlyView() : alm::rhi::BufferReadOnlyView{};
}

alm::rhi::BufferReadOnlyView alm::gfx::GpuSceneBuffers::GetInstancesBufferView(GpuSceneBuffersHandle handle) const
{
	assert(m_SceneStates.valid_index(handle.idx));
	return m_SceneStates[handle.idx].MeshInstancesBuffer ?
		m_SceneStates[handle.idx].MeshInstancesBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

alm::rhi::BufferReadOnlyView alm::gfx::GpuSceneBuffers::GetHeightmapPatchDataBufferView(GpuSceneBuffersHandle handle) const
{
	assert(m_SceneStates.valid_index(handle.idx));
	return m_SceneStates[handle.idx].HeightmapPatchDataBuffer ?
		m_SceneStates[handle.idx].HeightmapPatchDataBuffer->GetReadOnlyView() : rhi::BufferReadOnlyView{};
}

void alm::gfx::GpuSceneBuffers::UpdateGpuBuffers(rhi::ICommandList* commandList)
{
	UpdateGpuBufferGeneric<interop::MaterialData>(commandList, m_MaterialsBuffer, m_MaterialsState, "Materials GPU Buffer",
		[this](uint32_t matIdx, interop::MaterialData* dst)
		{
			SerializeMaterial(m_Materials[matIdx].get(), dst);
		});

	UpdateGpuBufferGeneric<interop::MeshData>(commandList, m_MeshesBuffer, m_MeshesState, "Meshes GPU Buffer",
		[this](uint32_t meshIdx, interop::MeshData* dst)
		{
			SerializeMesh(m_Meshes[meshIdx].get(), m_MeshMaterialIndices[meshIdx], dst);
		});

	for (auto& sceneState : m_SceneStates)
	{
		UpdateGpuBufferGeneric<interop::InstanceData>(commandList, sceneState.MeshInstancesBuffer, sceneState.MeshInstancesState,
			std::format("{}|Instances Buffer", sceneState.DebugName),
		[&](uint32_t miIdx, interop::InstanceData* dst)
		{
			SerializeMeshInstance(sceneState.MeshInstances[miIdx], dst);
		});
	}
}

void alm::gfx::GpuSceneBuffers::FlushTransients(GpuSceneBuffersHandle handle, rhi::ICommandList* commandList)
{
	assert(m_SceneStates.valid_index(handle.idx));
	auto& ss = m_SceneStates[handle.idx];

	if (ss.TransientsAllocated > 0)
	{
		assert(ss.TransientInstacesStagingBuffer);			
		ss.TransientInstacesStagingBuffer->Unmap();

		commandList->BeginMarker("Flush Transient Instances");

		commandList->PushBarrier(rhi::Barrier::Buffer(ss.MeshInstancesBuffer.get(),
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

		commandList->CopyBufferToBuffer(
			ss.MeshInstancesBuffer.get(), kStaticInstanceCount * sizeof(interop::InstanceData),
			ss.TransientInstacesStagingBuffer.get(), 0,
			ss.TransientsAllocated * sizeof(interop::InstanceData));

		commandList->PushBarrier(rhi::Barrier::Buffer(ss.MeshInstancesBuffer.get(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

		commandList->EndMarker();

		m_Device->ReleaseQueued(std::move(ss.TransientInstacesStagingBuffer));
		ss.TransientsAllocated = 0;
		ss.TransientInstancesDataPtr = nullptr;
	}

	if (ss.HeighmapPatchesAllocated > 0)
	{
		assert(ss.HeightmapPatchDataStagingBuffer && ss.HeightmapPatchDataBuffer);
		ss.HeightmapPatchDataStagingBuffer->Unmap();

		commandList->BeginMarker("Flush Heightmap Patches");

		commandList->PushBarrier(rhi::Barrier::Buffer(ss.HeightmapPatchDataBuffer.get(),
			rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

		commandList->CopyBufferToBuffer(
			ss.HeightmapPatchDataBuffer.get(), 0,
			ss.HeightmapPatchDataStagingBuffer.get(), 0,
			ss.HeighmapPatchesAllocated * sizeof(interop::HeightmapPatchData));

		commandList->PushBarrier(rhi::Barrier::Buffer(ss.HeightmapPatchDataBuffer.get(),
			rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

		commandList->EndMarker();

		m_Device->ReleaseQueued(std::move(ss.HeightmapPatchDataStagingBuffer));
		ss.HeighmapPatchesAllocated = 0;
		ss.HeightmapPatchDataPtr = nullptr;
	}
}

template<typename ElemT>
alm::rhi::BufferOwner alm::gfx::GpuSceneBuffers::CreateBuffer(uint32_t requiredElementCount, rhi::ResourceState initialState, const std::string& debugName)
{
	rhi::BufferDesc desc{
		.memoryAccess = rhi::MemoryAccess::Default,
		.shaderUsage = rhi::BufferShaderUsage::ReadOnly,
		.sizeBytes = requiredElementCount * sizeof(ElemT),
		.format = rhi::Format::UNKNOWN,
		.stride = sizeof(ElemT) };

	return m_Device->CreateBuffer(desc, initialState, debugName);
}

template<typename ElemT>
alm::rhi::ResourceState alm::gfx::GpuSceneBuffers::GrowBufferIfNeeded(
	alm::rhi::ICommandList* commandList,
	alm::rhi::BufferOwner& buffer,
	uint32_t requiredSize,
	const std::string& debugName)
{
	const uint32_t prevSize = buffer ? buffer->GetDesc().sizeBytes / sizeof(ElemT) : 0u;

	if (requiredSize <= prevSize)
		return rhi::ResourceState::SHADER_RESOURCE;  // no grow

	// No buffer grown is supported for the moment.
	// Buffers needs to be initialized at its default/max size
	// Kepping below code here because can be used as reference if we allow buffer grown again
	LOG_ERROR("Buffer '{}' overflow. Required: {}, capacity: {}. Resize statics in code.", debugName, requiredSize, prevSize);
	assert(0 && "Buffer overflow");
	std::abort();

#if 0
	rhi::BufferOwner oldBuffer = std::move(buffer);
	buffer = CreateBuffer<ElemT>(requiredSize * 2, rhi::ResourceState::COPY_DST, debugName);

	if (oldBuffer)
		commandList->CopyBufferToBuffer(buffer.get(), 0, oldBuffer.get(), 0, prevSize * sizeof(ElemT));

	return rhi::ResourceState::COPY_DST;
#endif
}

template<typename ElemT, typename Indices, typename SerializeFn>
void alm::gfx::GpuSceneBuffers::UploadIndices(rhi::ICommandList* commandList, rhi::IBuffer* dstBuffer, rhi::IBuffer* uploadBuffer, ElemT* uploadDataPtr,
	const Indices& indices, size_t& srcIdx, SerializeFn serializeFn)
{
	for (uint32_t elemIdx : indices)
	{
		serializeFn(elemIdx, uploadDataPtr + srcIdx);

		// TODO: Consider grouping consecutive indices in a single CopyBufferToBuffer call
		commandList->CopyBufferToBuffer(
			dstBuffer, elemIdx * sizeof(ElemT),
			uploadBuffer, srcIdx * sizeof(ElemT),
			sizeof(ElemT));

		++srcIdx;
	}
}

template<typename ElemT, typename SerializeFn>
void alm::gfx::GpuSceneBuffers::UpdateGpuBufferGeneric(rhi::ICommandList* commandList, rhi::BufferOwner& buffer, RefreshState& state,
	const std::string& debugName, SerializeFn&& serializeFn)
{
	// 1. Grow Gpu buffer if needed
	rhi::ResourceState bufferState = rhi::ResourceState::SHADER_RESOURCE;
	if (!state.NewIndices.empty())
	{
		uint32_t max_idx = *std::ranges::max_element(state.NewIndices);
		bufferState = GrowBufferIfNeeded<ElemT>(commandList, buffer, max_idx + 1, debugName);
	}

	// 2. Upload pending changes (adds + dirty)
	const size_t numItems = state.NewIndices.size() + state.DirtyIndices.size();
	if (numItems > 0)
	{
		assert(buffer);

		// First create upload staging buffer.
		// TODO: Since this is gonna be quite common (animations), consider having a single pre-allocated triple-buffered staging buffer
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.sizeBytes = numItems * sizeof(ElemT) };
		rhi::BufferOwner uploadBuffer = m_Device->CreateBuffer(desc, rhi::ResourceState::COPY_SRC, std::format("{} - Staging", debugName));

		if (bufferState == rhi::ResourceState::SHADER_RESOURCE)
		{
			commandList->PushBarrier(
				rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));
		}

		// Copy data to the staging buffer and request copy at the same time
		// TODO: Consider grouping consecutive indices in a single CopyBufferToBuffer call
		auto* uploadData = (ElemT*)uploadBuffer->Map();
		size_t stagingDataIdx = 0; // running counter

		UploadIndices<ElemT>(commandList, buffer.get(), uploadBuffer.get(), uploadData, state.NewIndices, stagingDataIdx, serializeFn);
		UploadIndices<ElemT>(commandList, buffer.get(), uploadBuffer.get(), uploadData, state.DirtyIndices, stagingDataIdx, serializeFn);

		uploadBuffer->Unmap();
		commandList->PushBarrier(
			rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));
	}

	state = {};

	// Note that state.RemovedIndices is not used for anything. Because removed indices require no action in terms of data modification.
	// We could surely remove it, but we are going to keep it on case we need it for something in the future.
}

std::pair<uint32_t, interop::InstanceData*> alm::gfx::GpuSceneBuffers::AllocateTransientInstances(GpuSceneBuffersHandle handle, uint32_t count)
{
	assert(count > 0);
	assert(m_SceneStates.valid_index(handle.idx));

	auto& ss = m_SceneStates[handle.idx];

	if (ss.TransientsAllocated + count > kTransientInstanceCount)
	{
		LOG_ERROR("Not enough space in the transient buffer.");
		return { UINT32_MAX, nullptr };
	}

	if (!ss.TransientInstacesStagingBuffer)
	{
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.sizeBytes = kTransientInstanceCount * sizeof(interop::InstanceData) };

		ss.TransientInstacesStagingBuffer = m_Device->CreateBuffer(desc, rhi::ResourceState::COPY_SRC, std::format("{}|Transient staging", ss.DebugName));
		ss.TransientInstancesDataPtr = (interop::InstanceData*)ss.TransientInstacesStagingBuffer->Map();
	}

	interop::InstanceData* ptr = ss.TransientInstancesDataPtr + ss.TransientsAllocated;
	uint32_t baseIdx = ss.TransientsAllocated + kStaticInstanceCount; // Adds kStaticInstanceCount because transients starts where statics end

	ss.TransientsAllocated += count;

	return { baseIdx, ptr };
}