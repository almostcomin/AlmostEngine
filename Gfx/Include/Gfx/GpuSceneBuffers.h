#pragma once

#include "Gfx/ResourceRefCount.h"
#include "Gfx/GpuSceneBuffersHandle.h"
#include "Gfx/MaterialDomain.h"
#include "RHI/ResourceState.h"
#include "RHI/ShaderViews.h"
#include "RHI/RasterizerState.h"
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
	struct TerrainMaterial;
}

namespace alm::gfx
{

class GpuSceneBuffers
{
public:

	static constexpr uint32_t kMaxStaticInstanceCount		= 8192;
	static constexpr uint32_t kMaxTransientInstanceCount	= 65536;
	static constexpr uint32_t kMaxMaterialCount				= 1024;
	static constexpr uint32_t kMaxTerrainMaterialCount		= 256;
	static constexpr uint32_t kMaxMeshRefCount				= 4096;

	enum class MaterialType
	{
		Undef,
		Standard,
		Heightmap
	};

	struct MaterialIndexEntry
	{
		uint32_t Index;
		MaterialType Type;
	};

	using MaterialRefCount = ResourceRefCount<const Material>;
	using TerrainMaterialRefCount = ResourceRefCount<const TerrainMaterial>;
	using MeshRefCount = ResourceRefCount<const Mesh>;

	using MeshInstanceLeafsContainer = alm::stable_vector<MeshInstance*, kMaxStaticInstanceCount>;
	using MaterialsContainer = alm::unique_stable_vector<MaterialRefCount, kMaxMaterialCount>;
	using TerrainMaterialsContainer = alm::unique_stable_vector<TerrainMaterialRefCount, kMaxTerrainMaterialCount>;
	using MeshesContainer = alm::unique_stable_vector<MeshRefCount, kMaxMeshRefCount>;
	using MeshMaterialIndicesContainer = std::array<MaterialIndexEntry, MeshesContainer::max_elements>;

	struct BucketInfo
	{
		uint32_t FirstBatchIndex;
		uint32_t BatchCount;
	};
	using BucketInfoArray = BucketInfo[(int)MaterialDomain::_Size][(int)rhi::CullMode::_Size];

	struct TransientRecord
	{
		MaterialDomain Domain;
		rhi::CullMode Cull;
		uint32_t FirstBatchIndex;
		uint32_t BatchCount;
		std::string DebugName;
	};

	struct HeightmapPatchesAllocation
	{
		uint32_t InstancesBaseIndex;
		uint32_t PatchesBaseIndex;
		uint32_t Count;
		interop::InstanceData* InstancesDataPtr;
		interop::InstanceCullData* CullDataBufferPtr;
		interop::BatchTableEntry* BatchTableEntriesPtr;
		interop::HeightmapPatchData* HeightmapPatchesPtr;
		uint32_t FirstBatchIndex;
		uint32_t FirstPayloadRegion;
	};

public:

	static constexpr uint32_t MaxInstances() { return kMaxStaticInstanceCount + kMaxTransientInstanceCount; }

	GpuSceneBuffers(rhi::Device* device);
	~GpuSceneBuffers();

	GpuSceneBuffersHandle RequestSceneHandle(const std::string& debugName);
	void ReleaseSceneHandle(GpuSceneBuffersHandle handle);

	uint32_t RegisterMaterial(const gfx::Material* mat);
	uint32_t RegisterTerrainMaterial(const gfx::TerrainMaterial* mat);

	uint32_t RegisterMesh(const gfx::Mesh* mesh, MaterialType materialType);

	uint32_t RegisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi);

	void UnregisterMaterial(const gfx::Material* mat);
	void UnregisterTerrainMaterial(const gfx::TerrainMaterial* mat);
	void UnregisterMesh(const gfx::Mesh* mesh);

	void UnregisterMaterial(uint32_t idx);
	void UnregisterTerrainMaterial(uint32_t idx);
	void UnregisterMesh(uint32_t idx);

	void UnregisterMeshInstance(GpuSceneBuffersHandle handle, gfx::MeshInstance* mi);

	void SetDirtyMaterial(const gfx::Material* mat);
	void SetDirtyMaterial(uint32_t idx);

	void SetDirtyTerrainMaterial(const gfx::TerrainMaterial* mat);
	void SetDirtyTerrainMaterial(uint32_t idx);

	void SetDirtyMesh(const gfx::Mesh* mesh);
	void SetDirtyMesh(uint32_t idx);

	void SetDirtyMeshInstance(GpuSceneBuffersHandle handle, const gfx::MeshInstance* mi);

	void RebindMeshMaterial(const Mesh* mesh, MaterialType materialType);

	HeightmapPatchesAllocation AllocateTransientHeightmapPatches(GpuSceneBuffersHandle handle, uint32_t count);

	const MaterialsContainer& GetMaterials() const { return m_Materials; }
	const MeshesContainer& GetMeshes() const { return m_Meshes; }

	MaterialIndexEntry GetMaterialIndexFromMesh(const gfx::Mesh* mesh) const;
	MaterialIndexEntry GetMaterialIndexFromMeshIndex(uint32_t meshIdx) const;

	rhi::BufferReadOnlyView GetMeshesBufferView() const;
	rhi::BufferReadOnlyView GetMaterialsBufferView() const;
	rhi::BufferReadOnlyView GetTerrainMaterialsBufferView() const;
	rhi::BufferReadOnlyView GetInstancesBufferView(GpuSceneBuffersHandle handle) const;
	rhi::BufferReadOnlyView GetInstancesCullDataBufferView(GpuSceneBuffersHandle handle) const;
	rhi::BufferReadOnlyView GetHeightmapPatchDataBufferView(GpuSceneBuffersHandle handle) const;
	rhi::BufferReadOnlyView GetBatchTableBufferView(GpuSceneBuffersHandle handle) const;

	// Returns kMaxStaticInstanceCount + TransientsAllocated.
	// The static part may contain holes, so the whole buffer must be processed.
	// The transient part is contiguous, so we can stop at the current allocation.
	size_t GetRenderInstancesCount(GpuSceneBuffersHandle handle) const;

	size_t GetBatchTableSize(GpuSceneBuffersHandle handle) const;

	const BucketInfoArray* GetBucketInfo(GpuSceneBuffersHandle handle) const;
	std::span<const TransientRecord> GetTransientRecords(GpuSceneBuffersHandle handle) const;

	void UpdateGpuBuffers(rhi::ICommandList* commandList);

	void ResetTransients(GpuSceneBuffersHandle handle);
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
		MeshInstanceLeafsContainer MeshInstances;						// Only static (not transient instances)
		RefreshState MeshInstancesState;								// Deferred updates for the static instance buffer

		rhi::BufferOwner MeshInstancesBuffer;							// interop::InstanceData (static + transient)
		rhi::BufferOwner TransientInstacesStagingBuffer;				// Staging buffer for transient instances
		interop::InstanceData* TransientInstancesDataPtr = nullptr;		// Cached pointer to TransientInstacesStagingBuffer mapped ptr

		rhi::BufferOwner MeshInstanceCullDataBuffer;					// interop::InstanceCullData (static + transient). 1:1 with MeshInstancesBuffer
		rhi::BufferOwner TransientInstanceCullDataStagingBuffer;		// Staging buffer for transient instances - cull data
		interop::InstanceCullData* TransientInstanceCullDataPtr = nullptr; // Cached pointer to TransientInstanceCullDataBuffer mapped ptr

		rhi::BufferOwner HeightmapPatchDataBuffer;						// interop::HeightmapPatchData (transient only)
		rhi::BufferOwner HeightmapPatchDataStagingBuffer;				// Staging buffer for heightmap patch data
		interop::HeightmapPatchData* HeightmapPatchDataPtr = nullptr;	// Cached pointer to HeightmapPatchDataStagingBuffer mapped ptr

		uint32_t TransientsAllocated = 0;								// Next index free in the transient instances region
		uint32_t HeighmapPatchesAllocated = 0;							// Next index free in the HeightmapPatch buffer
		std::vector<TransientRecord> TransientRecords;

		// One entry per (domain, cullMode, batchKey)
		std::vector<interop::BatchTableEntry> BatchTable;
		// GPU version of BatchTable
		rhi::BufferOwner BatchTableBuffer;
		rhi::BufferOwner TransientBatchTableStagingBuffer;
		interop::BatchTableEntry* TransientBatchTablePtr = nullptr;

		// Used by MaterialPassRenderer::DrawIndirect indicates the number of instances that share (domain, cullmode)
		// that is, the number of instances that can be draw with a single call to ExecuteIndirect since they share PSO.
		// It also indicates the location (FirstBatchIndex) in the args buffer of the bucket's first batch command.
		BucketInfoArray Buckets;
		// If a new instance is added or an existing one is removed or any material property changes, this invalidates
		// the whole BatchTable
		bool BatchLayoutDirty = true;

		// Number of static entries in the BatchTableBuffer.
		// The BatchTableBuffer contains both static + transients instances.
		// StaticBatchCount marks the end of the static instances and the begining of the transient ones.
		uint32_t StaticBatchCount = 0;
		// Number of static instances in the BatchTable. It *should* be the same as MeshInstances.size(),
		// but invalid (not registered) isntances are removed.
		uint32_t StaticPayloadTotal = 0;

		bool DataInitialized = false;

		std::string DebugName;
	};

	struct TransientInstanceAllocation
	{
		uint32_t BaseInstanceIndex; // Slot in the MeshInstancesBuffer & MeshInstanceCullDataBuffer
		interop::InstanceData* InstancesBufferPtr;
		interop::InstanceCullData* CullDataBufferPtr;
		interop::BatchTableEntry* BatchTableBufferPtr;
		uint32_t FirstBatchIndex;
		uint32_t FirstPayloadRegion;
	};

private:

	template<typename ElemT>
	rhi::BufferOwner CreateBuffer(uint32_t requiredElementCount, rhi::ResourceState initialState, const std::string& debugName);

	template<typename ElemT>
	rhi::ResourceState GrowBufferIfNeeded(rhi::ICommandList* commandList, rhi::BufferOwner& buffer, uint32_t requiredElementCount,
		const std::string& debugName);

	template<typename ElemT, typename Indices, typename SerializeFn>
	void UploadIndices(rhi::ICommandList* commandList, rhi::IBuffer* dstBuffer, rhi::IBuffer* uploadBuffer, ElemT* uploadDataPtr,
		const Indices& indices,
		size_t& srcIdx, SerializeFn serializeFn);

	template<typename ElemT, typename SerializeFn, typename InvalidateFn = std::nullptr_t>
	void UpdateGpuBufferGeneric(rhi::ICommandList* commandList, rhi::BufferOwner& buffer, const RefreshState& state, const std::string& debugName,
		SerializeFn&& serializeFn, InvalidateFn&& invalidateFn = nullptr);

	// 1 batch = 1 payload slot = 1 transient instance
	TransientInstanceAllocation AllocateTransientInstances(GpuSceneBuffersHandle handle, uint32_t count, MaterialDomain domain, rhi::CullMode cullMode,
		const std::string& optDebugName = {});

	void RebuildBatchTable(SceneState& ss);
	void UploadBatchTable(SceneState& ss, rhi::ICommandList* commandList);

	void InitializeMeshInstanceCullData(SceneState& ss, rhi::ICommandList* commandList);

private:

	MaterialsContainer m_Materials;
	TerrainMaterialsContainer m_TerrainMaterials;
	MeshesContainer m_Meshes;
	MeshMaterialIndicesContainer m_MeshMaterialIndices;

	alm::stable_vector<SceneState, 32> m_SceneStates;

	RefreshState m_MaterialsState;
	RefreshState m_TerrainMaterialsState;
	RefreshState m_MeshesState;

	rhi::BufferOwner m_MaterialsBuffer;				// interop::MaterialData
	rhi::BufferOwner m_TerrainMaterialsBuffer;		// interop::TerrainMaterialData
	rhi::BufferOwner m_MeshesBuffer;				// interop::MeshData

	rhi::Device* m_Device;
};

} // namespace alm::gfx