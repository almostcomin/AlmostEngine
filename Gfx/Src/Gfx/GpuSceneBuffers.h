#pragma once

#include "Gfx/ResourceRefCount.h"
#include "Gfx/GpuSceneBuffersHandle.h"
#include "RHI/ResourceState.h"
#include "RHI/ShaderViews.h"
#include "Interop/RenderResources.h"

namespace alm::rhi
{
	class Device;
	class ICommandList;
}

namespace alm::gfx
{
	class Material;
	class Mesh;
	class MeshInstance;
}

namespace alm::gfx
{

class GpuSceneBuffers
{
public:

	static constexpr size_t kStaticInstanceCount	= 8192;
	static constexpr size_t kTransientInstanceCount	= 4096;
	static constexpr size_t kMaterialCount			= 1024;
	static constexpr size_t kMeshRefCount			= 4096;

	using MaterialRefCount = ResourceRefCount<const Material>;
	using MeshRefCount = ResourceRefCount<const Mesh>;

	using MeshInstanceLeafsContainer = alm::stable_vector<MeshInstance*, kStaticInstanceCount>;
	using MaterialsContainer = alm::unique_stable_vector<MaterialRefCount, kMaterialCount>;
	using MeshesContainer = alm::unique_stable_vector<MeshRefCount, kMeshRefCount>;
	using MeshMaterialIndicesContainer = std::array<uint32_t, MeshesContainer::max_elements>;

	struct TransientAllocation
	{
		uint32_t BaseIndex;
		uint32_t Count;
		interop::InstanceData* DataPtr;
	};

public:

	GpuSceneBuffers(rhi::Device* device);
	~GpuSceneBuffers();

	GpuSceneBuffersHandle RequestSceneHandle(const std::string& debugName);
	void ReleaseSceneHandle(GpuSceneBuffersHandle handle);

	uint32_t RegisterMaterial(const gfx::Material* mat);
	uint32_t RegisterMesh(const gfx::Mesh* mesh);
	uint32_t RegisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi);

	void UnregisterMaterial(const gfx::Material* mat);
	void UnregisterMesh(const gfx::Mesh* mesh);
	void UnregisterMaterial(uint32_t idx);
	void UnregisterMesh(uint32_t idx);

	void UnregisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi);

	void SetDirtyMaterial(const gfx::Material* mat);
	void SetDirtyMesh(const gfx::Mesh* mesh);
	void SetDirtyMaterial(uint32_t idx);
	void SetDirtyMesh(uint32_t idx);

	void SetDirtyMeshInstance(GpuSceneBuffersHandle handle, const gfx::MeshInstance* mi);

	void RebindMeshMaterial(const Mesh* mesh);

	TransientAllocation AllocateTransientInstances(GpuSceneBuffersHandle handle, uint32_t count);

	const MaterialsContainer& GetMaterials() const { return m_Materials; }
	// Registering a Mesh also register its material
	const MeshesContainer& GetMeshes() const { return m_Meshes; }

	uint32_t GetMaterialIndexFromMesh(const gfx::Mesh* mesh) const;
	uint32_t GetMaterialIndexFromMeshIdx(uint32_t meshIdx) const;

	rhi::BufferReadOnlyView GetMeshesBufferView() const;
	rhi::BufferReadOnlyView GetMaterialsBufferView() const;
	rhi::BufferReadOnlyView GetInstancesBufferView(GpuSceneBuffersHandle handle) const;

	void UpdateGpuBuffers(rhi::ICommandList* commandList);
	void FlushTransients(GpuSceneBuffersHandle handle, rhi::ICommandList* commandList);

private:

	struct RefreshState
	{
		alm::unique_vector<uint32_t> NewIndices;
		alm::unique_vector<uint32_t> RemovedIndices;
		alm::unique_vector<uint32_t> DirtyIndices;
	};

	struct SceneState
	{
		MeshInstanceLeafsContainer MeshInstances;
		RefreshState MeshInstancesState;
		rhi::BufferOwner MeshInstancesBuffer;
		rhi::BufferOwner TransientInstacesStagingBuffer;
		uint32_t TransientAllocated = 0;
		interop::InstanceData* TransientDataPtr = nullptr;
		std::string DebugName;
	};

private:

	template<typename ElemT>
	rhi::BufferOwner CreateBuffer(uint32_t requiredElementCount, rhi::ResourceState initialState, const char* debugName);

	template<typename ElemT>
	rhi::ResourceState GrowBufferIfNeeded(rhi::ICommandList* commandList, rhi::BufferOwner& buffer, uint32_t requiredElementCount, const char* debugName);

	template<typename ElemT, typename Indices, typename SerializeFn>
	void UploadIndices(rhi::ICommandList* commandList, rhi::IBuffer* dstBuffer, rhi::IBuffer* uploadBuffer, ElemT* uploadDataPtr, const Indices& indices,
		size_t& srcIdx, SerializeFn serializeFn);

	template<typename ElemT, typename SerializeFn>
	void UpdateGpuBufferGeneric(rhi::ICommandList* commandList, rhi::BufferOwner& buffer, RefreshState& state, const char* debugName,
		SerializeFn&& serializeFn);

private:

	MaterialsContainer m_Materials;
	MeshesContainer m_Meshes;
	MeshMaterialIndicesContainer m_MeshMaterialIndices;

	alm::stable_vector<SceneState, 32> m_SceneStates;

	RefreshState m_MaterialsState;
	RefreshState m_MeshesState;

	rhi::BufferOwner m_MaterialsBuffer;		// interop::MaterialData
	rhi::BufferOwner m_MeshesBuffer;		// interop::MeshData

	rhi::Device* m_Device;
};

} // namespace alm::gfx