#include "Gfx/GfxPCH.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/SceneHeigthmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/Camera.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Transform.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"

/* Level & cells meaning
*
*	Level 0: 1×1 cells          Level 1 : 2×2 cells         Level 2 : 4×4 cells
*	┌───────────────┐            ┌───────┬───────┐           ┌────┬────┬────┬────┐
*	│               │            │(0, 0) │(1, 0) │           │0, 0│1, 0│2, 0│3, 0│
*	│               │            ├───────┼───────┤           ├────┼────┼────┼────┤
*	│	 (0, 0)		│            │(0, 1) │(1, 1) │           │0, 1│1, 1│2, 1│3, 1│
*	│               │            └───────┴───────┘           ├────┼────┼────┼────┤
*	│               │                                        │0, 2│1, 2│2, 2│3, 2│
*	└───────────────┘                                        ├────┼────┼────┼────┤
*															 │0, 3│1, 3│2, 3│3, 3│
*															 └────┴────┴────┴────┘
*/

alm::gfx::HeightmapInstance::QuadNodeCoord alm::gfx::HeightmapInstance::QuadNodeCoord::Child(int i) const
{
	assert(i >= 0 && i < 4);

	// Morton code (y:x), x is bit 0, y is bit 1
	const int offsetX = i & 1;
	const int offsetY = (i >> 1) & 1;

	QuadNodeCoord result;
	result.Level = Level + 1;
	result.CellIndex.x = CellIndex.x * 2 + offsetX;
	result.CellIndex.y = CellIndex.y * 2 + offsetY;

	return result;
}

alm::gfx::HeightmapInstance::QuadNodeCoord alm::gfx::HeightmapInstance::QuadNodeCoord::Parent() const
{
	assert(Level > 0);

	QuadNodeCoord result;
	result.Level = Level - 1;
	result.CellIndex.x = CellIndex.x >> 1;
	result.CellIndex.y = CellIndex.y >> 1;

	return result;
}

float alm::gfx::HeightmapInstance::QuadNodeCoord::CellSize() const
{
	return 1.0f / float(1u << Level);   // 1 / 2^Level
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::MinUV() const
{
	return float2(CellIndex.x, CellIndex.y) * CellSize();
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::MaxUV() const
{
	return MinUV() + CellSize();
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::CenterUV() const
{
	return MinUV() + CellSize() / 2.f;
}

alm::aabox3f alm::gfx::HeightmapInstance::QuadNodeCoord::Bounds(float minY, float maxY) const
{
	const float cellSize = CellSize();
	const float2 minUV = MinUV();
	const float2 maxUV = MaxUV();
	return aabox3f{
		float3(minUV.x, minY, minUV.y),
		float3(maxUV.x, maxY, maxUV.y)
	};
}

alm::gfx::HeightmapInstance::HeightmapInstance(const SceneHeightmap* sceneHeightmap, DeviceManager* deviceManager) :
	m_MaxLevel{ 4 }, m_SceneHeightmap{ sceneHeightmap }, m_DeviceManager{ deviceManager }
{}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

void alm::gfx::HeightmapInstance::Update(const Camera* camera, GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle)
{
	auto* uploadBuffer = m_DeviceManager->GetUploadBuffer();
	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
	rhi::TextureSampledView textureView = heightmap->GetHeightsTexture()->GetSampledView();

	m_LeafNodes.clear();
	QuadNodeCoord root{
		.Level = 0, .CellIndex{0, 0} };

	SelectLODNodes(root, camera);
	if (m_LeafNodes.empty())
		return;

	auto [patchDataRawPtr, patchDataBufferOffset] = 
		uploadBuffer->RequestSpaceForBufferDataUpload(m_LeafNodes.size() * sizeof(interop::HeightmapPatchData));
	auto* patchDataPtr = (interop::HeightmapPatchData*)patchDataRawPtr;

	GpuSceneBuffers::HeightmapPatchesAllocation alloc = gpuSceneBuffers->AllocateTransientHeightmapPatches(gpuBuffersHandle, m_LeafNodes.size());
	for (size_t i = 0; i < m_LeafNodes.size(); ++i)
	{
		const QuadNodeCoord& node = m_LeafNodes[i];
		const float2 minUV = node.MinUV();
		const float cellSize = node.CellSize();

		Transform localTransform;
		localTransform.SetTranslation({ minUV.x, 0.f, minUV.y });
		localTransform.SetScale({ cellSize, 1.f, cellSize });

		float4x4 worldMatrix = m_SceneHeightmap->GetWorldTransform() * localTransform.GetMatrix();
		
		alloc.InstancesDataPtr[i].modelMatrix = worldMatrix;
		alloc.InstancesDataPtr[i].inverseModelMatrix = glm::inverse(worldMatrix);
		
		alloc.HeightmapPatchesPtr[i].MinUV = minUV;
		alloc.HeightmapPatchesPtr[i].CellSize = cellSize;
		alloc.HeightmapPatchesPtr[i].HeightmapTextureDI = textureView;
	}
	m_InstancesAllocBaseIdx = alloc.InstancesBaseIndex;
	m_PatchesAllocBaseIndex = alloc.PatchesBaseIndex;
}

void alm::gfx::HeightmapInstance::CollectDrawInfos(const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const
{
	for (int i = 0; i < m_LeafNodes.size(); ++i)
	{
		out.push_back(RenderableDrawInfo{
			.MaterialDomain = MaterialDomain::Terrain,
			.CullMode = rhi::CullMode::Back,
			.BatchKey = reinterpret_cast<uintptr_t>(this),
			.InstanceIdx = m_InstancesAllocBaseIdx + i,
			.MeshIndex = m_SceneHeightmap->GetPatchMeshGpuIndex(),
			.MaterialIndex = gpuSceneBuffers->GetMaterialIndexFromMeshIdx(m_SceneHeightmap->GetPatchMeshGpuIndex()),
			.TransientBaseIndex = m_PatchesAllocBaseIndex,
			.IndexCount = m_SceneHeightmap->GetHeightmap()->GetPatchIndicesCount() });
	}
}

bool alm::gfx::HeightmapInstance::ShouldSubdivide(const QuadNodeCoord& coord, const aabox3f& worldBounds, const Camera* camera)
{	
	const float3 worldCenter = worldBounds.center();
	const float dist = glm::distance(camera->GetPosition(), worldCenter);
	const float3 d = worldBounds.diagonal();
	const float2 dXZ = float2{ d.x, d.z };
	const float size = glm::length(dXZ);

	return dist < size * m_LODDistanceFactor;
}

void alm::gfx::HeightmapInstance::SelectLODNodes(const QuadNodeCoord& coord, const Camera* camera)
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	// Data out of range (not tileable only)
	const auto& dataSource = heightmap->GetSource();
	if (!dataSource->IsTileable())
	{
		float2 dataNormSize = dataSource->GetNormalizedSize();
		float2 minUV = coord.MinUV();
		if (minUV.x >= dataNormSize.x || minUV.y >= dataNormSize.y)
			return;
	}

	// Check frustum
	const float2 heightRange = dataSource->GetHeightRange();
	aabox3f localBounds = coord.Bounds(heightRange.x, heightRange.y);

	aabox3f worldBounds = localBounds.transform(m_SceneHeightmap->GetWorldTransform());
	if (!camera->GetFrustum().test(worldBounds))
		return;

	// Predicate
	if (coord.Level < m_MaxLevel && ShouldSubdivide(coord, worldBounds, camera))
	{
		for (int i = 0; i < 4; ++i)
		{
			SelectLODNodes(coord.Child(i), camera);
		}
	}
	else
	{
		m_LeafNodes.push_back(coord);
	}
}