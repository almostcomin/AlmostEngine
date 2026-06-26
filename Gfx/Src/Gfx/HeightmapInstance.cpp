#include "Gfx/GfxPCH.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/SceneHeightmap.h"
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

float alm::gfx::HeightmapInstance::QuadNodeCoord::SizeUV() const
{
	return 1.0f / float(1u << Level);   // 1 / 2^Level
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::MinUV() const
{
	return float2(CellIndex.x, CellIndex.y) * SizeUV();
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::MaxUV() const
{
	return MinUV() + SizeUV();
}

float2 alm::gfx::HeightmapInstance::QuadNodeCoord::CenterUV() const
{
	return MinUV() + SizeUV() / 2.f;
}

alm::aabox3f alm::gfx::HeightmapInstance::QuadNodeCoord::Bounds(float sizeFactor, float minY, float maxY) const
{
	const float2 minUV = MinUV() * sizeFactor;
	const float2 maxUV = MaxUV() * sizeFactor;
	return aabox3f{
		float3(minUV.x, minY, minUV.y),
		float3(maxUV.x, maxY, maxUV.y)
	};
}

alm::gfx::HeightmapInstance::HeightmapInstance(const SceneHeightmap* sceneHeightmap) :
	m_SceneHeightmap{ sceneHeightmap }
{
	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
	m_MaxLevel = heightmap->GetMaxDepthLevel();
}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

void alm::gfx::HeightmapInstance::Update(const Camera* camera, const uint2& fbSize, GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle)
{
	if (m_Frozen)
	{
		FillGpuBuffers(gpuSceneBuffers, gpuBuffersHandle);
		return;
	}

	m_LeafNodes.clear();
	QuadNodeCoord root{
		.Level = 0, .CellIndex{0, 0} };

	SelectLODNodes(m_LeafNodes, root, camera, fbSize);
	if (m_LeafNodes.empty())
		return;

	// Calc mesh variant
	std::unordered_set<QuadNodeCoord, QuadNodeCoordHash> leafSet;
	leafSet.reserve(m_LeafNodes.size());
	for (const auto& c : m_LeafNodes)
		leafSet.insert(c);

	for (QuadNodeCoord& coord : m_LeafNodes)
	{
		Heightmap::PatchEdgeConfig edgeConfig;

		edgeConfig.North = GetEdgeMode(leafSet, coord, Axis::North);
		edgeConfig.South = GetEdgeMode(leafSet, coord, Axis::South);
		edgeConfig.East = GetEdgeMode(leafSet, coord, Axis::East);
		edgeConfig.West = GetEdgeMode(leafSet, coord, Axis::West);

		coord.patchVariantIdx = Heightmap::EdgeConfigToVariantIndex(edgeConfig);  // 0..15

		// EdgeMask:
		{
			// Corners: coord is (L, x, y)
			//			NE is (L, x+1, y+1)
			//			NW is (L, x-1, y+1)
			//			SE is (L, x+1, y-1)
			//			SW is (L, x-1, y-1)
			auto findCornerNeighbourLevel = [&](int dx, int dy) -> uint32_t
			{
				int32_t nx = (int32_t)coord.CellIndex.x + dx;
				int32_t ny = (int32_t)coord.CellIndex.y + dy;
				const int32_t cellsPerSide = 1 << coord.Level;
				if (nx < 0 || nx >= cellsPerSide || ny < 0 || ny >= cellsPerSide)
					return UINT32_MAX;

				QuadNodeCoord cornerCoord{ coord.Level, { (uint32_t)nx, (uint32_t)ny } };
				if (leafSet.count(cornerCoord) > 0)
					return coord.Level;

				if (coord.Level > 0)
				{
					QuadNodeCoord parent = cornerCoord.Parent();
					if (leafSet.count(parent) > 0)
						return parent.Level;
				}
				return UINT32_MAX;
			};

			uint32_t levelNE = findCornerNeighbourLevel( 1,  1);
			uint32_t levelNW = findCornerNeighbourLevel(-1,  1);
			uint32_t levelSE = findCornerNeighbourLevel( 1, -1);
			uint32_t levelSW = findCornerNeighbourLevel(-1, -1);

			// bits 0-3 edge mode:		bit0 = N Low?,  bit1 = S Low?,  bit2 = E Low?,  bit3 = W Low?
			// bits 4-7 corner mode:	bit4 = NE Low?, bit5 = NW Low?, bit6 = SE Low?, bit7 = SW Low?
			coord.edgeMask = 0;
			coord.edgeMask |= (edgeConfig.North == Heightmap::EdgeMode::Low) ? (1u << 0) : 0u;
			coord.edgeMask |= (edgeConfig.South == Heightmap::EdgeMode::Low) ? (1u << 1) : 0u;
			coord.edgeMask |= (edgeConfig.East  == Heightmap::EdgeMode::Low) ? (1u << 2) : 0u;
			coord.edgeMask |= (edgeConfig.West  == Heightmap::EdgeMode::Low) ? (1u << 3) : 0u;
			coord.edgeMask |= (levelNE < coord.Level) ? (1u << 4) : 0u;
			coord.edgeMask |= (levelNW < coord.Level) ? (1u << 5) : 0u;
			coord.edgeMask |= (levelSE < coord.Level) ? (1u << 6) : 0u;
			coord.edgeMask |= (levelSW < coord.Level) ? (1u << 7) : 0u;
		}
	}	

	// We have to sort by variant so patches with the same variant (mesh) are consecutive
	// in the gpu buffers
	std::ranges::sort(m_LeafNodes, [](const QuadNodeCoord& l, const QuadNodeCoord& r) 
		{ return l.patchVariantIdx < r.patchVariantIdx; });

	FillGpuBuffers(gpuSceneBuffers, gpuBuffersHandle);
}

void alm::gfx::HeightmapInstance::CollectDrawInfos(const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const
{
	for (int i = 0; i < m_LeafNodes.size(); ++i)
	{
		const uint32_t meshIndex = m_SceneHeightmap->GetPatchMeshGpuIndex(m_LeafNodes[i].patchVariantIdx);

		out.push_back(RenderableDrawInfo{
			.MaterialDomain = MaterialDomain::Terrain,
			.CullMode = rhi::CullMode::Back,
			.BatchKey = (reinterpret_cast<uintptr_t>(this) << 16) | uintptr_t(meshIndex),
			.InstanceIdx = m_InstancesAllocBaseIdx + i,
			.MeshIndex = meshIndex,
			.MaterialIndex = gpuSceneBuffers->GetMaterialIndexFromMeshIdx(meshIndex).Index,
			.TransientBaseIndex = m_PatchesAllocBaseIndex + i,
			.IndexCount = m_SceneHeightmap->GetHeightmap()->GetPatchIndicesCount(m_LeafNodes[i].patchVariantIdx) });
	}
}

std::string alm::gfx::HeightmapInstance::DumpTesselationInfo() const
{
	std::vector<QuadNodeCoord> leafNodes = m_LeafNodes;
	std::ranges::sort(leafNodes, [](const QuadNodeCoord& l, const QuadNodeCoord& r)
	{
		return std::tuple(l.Level, l.CellIndex.x, l.CellIndex.y) <
			std::tuple(r.Level, r.CellIndex.x, r.CellIndex.y);
	});

	std::stringstream ss;
	int level = -1;
	for (int i = 0; i < leafNodes.size(); ++i)
	{
		if (leafNodes[i].Level != level)
		{
			level = leafNodes[i].Level;
			ss << "\n--- Level " << level << "---\n";
		}
		ss << "[" << leafNodes[i].CellIndex.x << ", " << leafNodes[i].CellIndex.y << "] ";
	}

	ss << "\n";

	return ss.str();
}

alm::aabox3f alm::gfx::HeightmapInstance::GetAABoxWorldSpace(const QuadNodeCoord& node) const
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	float2 heightRange = heightmap->GetPatchHeightRange(node.Level, node.CellIndex);

	aabox3f localBounds = node.Bounds(heightmap->GetVirtualSize(), heightRange.x, heightRange.y);
	aabox3f worldBounds = localBounds.transform(m_SceneHeightmap->GetWorldTransform());

	return worldBounds;
}

bool alm::gfx::HeightmapInstance::ShouldSubdivide(const QuadNodeCoord& coord, const Camera* camera, const uint2& fbSize)
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	const float errWorld = heightmap->GetPatchErrorValue(coord.Level, coord.CellIndex);
#if 0
	return errWorld > 0.001f;
#else
	// Distance to the closest point of the AABB projected onto the camera forward.
	const float3 camPos = camera->GetPosition();
	const float3 camForward = camera->GetForward();
	const aabox3f worldBounds = GetAABoxWorldSpace(coord);
	const float3 closest = worldBounds.closestPoint(camPos);

	constexpr float kMinDepth = 0.1f;
	const float depth = std::max(kMinDepth, glm::dot(closest - camPos, camForward));

	const float focalPixels = ((float)fbSize.y / 2) / std::tan(camera->GetVerticalFOV() / 2);

	const float errScreen = errWorld * focalPixels / depth;

	// Hysteresis: penalize subdividing less when we are already a leaf (so we don't
	// immediately collapse back) and benefit subdividing more when we are not a leaf.
	// Values around 0.7 / 1.3 work well in practice; tune if popping persists.
	//const bool isLeaf = std::find(m_LeafNodes.begin(), m_LeafNodes.end(), coord) != m_LeafNodes.end();
	//const float hysteresis = isLeaf ? 0.7f : 1.3f;
	const float hysteresis = 1.f;

	return errScreen * hysteresis > m_LODDistanceFactor;
#endif
}

bool alm::gfx::HeightmapInstance::NeighborWouldForceSubdivide(const QuadNodeCoord& coord, Axis axis, const Camera* camera, const uint2& fbSize)
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	// Find same level neighbour coords
	int32_t nx = (int32_t)coord.CellIndex.x;
	int32_t ny = (int32_t)coord.CellIndex.y;
	switch (axis)
	{
	case Axis::West:  nx -= 1; break;
	case Axis::East:  nx += 1; break;
	case Axis::South: ny -= 1; break;
	case Axis::North: ny += 1; break;
	}
	const int32_t cellsPerSide = 1 << coord.Level;
	if (nx < 0 || nx >= cellsPerSide || ny < 0 || ny >= cellsPerSide)
		return false;  // at world's edge

	QuadNodeCoord neighbor{ coord.Level, { (uint32_t)nx, (uint32_t)ny } };

	// If neighbour doesn't want to subdivide we have finished
	if (!ShouldSubdivide(neighbor, camera, fbSize))
		return false;

    int borderChildren[2];
    switch (axis) {
        case Axis::West:  borderChildren[0] = 1; borderChildren[1] = 3; break;
        case Axis::East:  borderChildren[0] = 0; borderChildren[1] = 2; break;
        case Axis::South: borderChildren[0] = 2; borderChildren[1] = 3; break;
        case Axis::North: borderChildren[0] = 0; borderChildren[1] = 1; break;
    }

	// Any of his grandchildren wants?
	for (int b : borderChildren)
	{
		QuadNodeCoord child = neighbor.Child(b);
		if (ShouldSubdivide(child, camera, fbSize))
			return true;
	}

	return false;
}

void alm::gfx::HeightmapInstance::SelectLODNodes(std::vector<QuadNodeCoord>& leafNodes, const QuadNodeCoord& coord,
	const Camera* camera, const uint2& fbSize)
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
	const auto& dataSource = heightmap->GetSource();

	// Data out of range (not tileable only)
	if (!dataSource->IsTileable())
	{
		float2 minUV = coord.MinUV() * heightmap->GetUVScale();
		if (minUV.x >= 1.f || minUV.y > 1.f)
			return;
	}

	if (coord.CellIndex.x == 0 && coord.CellIndex.y == 2 && coord.Level == 2)
	{
		puts("hola");
	}
	if (coord.CellIndex.x == 0 && coord.CellIndex.y == 3 && coord.Level == 2)
	{
		puts("hola");
	}

	// Check frustum
	aabox3f worldBounds = GetAABoxWorldSpace(coord);
	if (!camera->GetFrustum().test(worldBounds))
		return;

	bool shouldSubdivide = coord.Level < m_MaxLevel && ShouldSubdivide(coord, camera, fbSize);
	if (!shouldSubdivide && coord.Level < m_MaxLevel)
	{
		for (Axis axis : { Axis::North, Axis::South, Axis::East, Axis::West })
		{
			if (NeighborWouldForceSubdivide(coord, axis, camera, fbSize))
			{
				shouldSubdivide = true;
				break;
			}
		}
	}

	if (shouldSubdivide)
	{
		for (int i = 0; i < 4; ++i)
		{
			SelectLODNodes(leafNodes, coord.Child(i), camera, fbSize);
		}
	}
	else
	{
		leafNodes.push_back(coord);
	}
}

alm::gfx::Heightmap::EdgeMode alm::gfx::HeightmapInstance::GetEdgeMode(const std::unordered_set<QuadNodeCoord, QuadNodeCoordHash>& leafSet,
	const QuadNodeCoord& coord, Axis axis)
{
	// 1. Calc neightbour coords of the same level than coord
	int32_t nx = (int32_t)coord.CellIndex.x;
	int32_t ny = (int32_t)coord.CellIndex.y;
	switch (axis) 
	{
	case Axis::West:  nx -= 1; break;
	case Axis::East:  nx += 1; break;
	case Axis::South: ny -= 1; break;
	case Axis::North: ny += 1; break;
	}

	// 2. Check if it is outside world border
	const int32_t cellsPerSide = 1 << coord.Level;
	if (nx < 0 || nx >= cellsPerSide || ny < 0 || ny >= cellsPerSide)
		return Heightmap::EdgeMode::Normal;

	QuadNodeCoord neighbor{
		.Level = coord.Level,
		.CellIndex = { (uint32_t)nx, (uint32_t)ny }
	};

	// 3. Check if neightbour exists as a leaf
	if (leafSet.count(neighbor) > 0)
		return Heightmap::EdgeMode::Normal;

	// 4. Check parent level (restricted quadtree guarantees diff is not higher than 1).
	if (coord.Level > 0)
	{
		QuadNodeCoord parent = neighbor.Parent();
		if (leafSet.count(parent) > 0)
			return Heightmap::EdgeMode::Low;
	}

	// 5. No neightbour leaf found with the same level or parent's level.
	return Heightmap::EdgeMode::Normal;
}

void alm::gfx::HeightmapInstance::FillGpuBuffers(GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle)
{
	Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
	const auto& dataSource = heightmap->GetSource();
	rhi::TextureSampledView textureView = heightmap->GetHeightsTexture()->GetSampledView();
	const rhi::TextureDesc& texDesc = heightmap->GetHeightsTexture()->GetDesc();

	const float4x4 nodeWorldMatrix = m_SceneHeightmap->GetWorldTransform();

	const float4x4 inverseNodeWorldMatrix = glm::inverse(nodeWorldMatrix);

	alm::gfx::Transform hmTransform;
	const float2 scale = 1.f / heightmap->GetActualSize();
	hmTransform.SetScale(float3{ scale.x, 1.f, scale.y });
	const float4x4 inverseHeightmapMatrix = hmTransform.GetMatrix() * inverseNodeWorldMatrix;

	const float3x3 nodeNormalMatrix = glm::transpose(glm::mat3(inverseNodeWorldMatrix));

	float2 uvScale = heightmap->GetUVScale();

	GpuSceneBuffers::HeightmapPatchesAllocation alloc = gpuSceneBuffers->AllocateTransientHeightmapPatches(gpuBuffersHandle, m_LeafNodes.size());
	for (size_t i = 0; i < m_LeafNodes.size(); ++i)
	{
		const QuadNodeCoord& coord = m_LeafNodes[i];
		const float2 minUV = coord.MinUV();
		const float sizeUV = coord.SizeUV();

		Transform localTransform;
		localTransform.SetTranslation(float3{ minUV.x, 0.f, minUV.y } * heightmap->GetVirtualSize());
		localTransform.SetScale(float3{ sizeUV * heightmap->GetVirtualSize(), 1.f, sizeUV * heightmap->GetVirtualSize() });

		const float4x4 worldMatrix = nodeWorldMatrix * localTransform.GetMatrix();
		const float4x4 inverseWorldMatrix = glm::inverse(worldMatrix);

		alloc.InstancesDataPtr[i].modelMatrix = worldMatrix;
		alloc.InstancesDataPtr[i].inverseModelMatrix = inverseWorldMatrix;
		alloc.HeightmapPatchesPtr[i].MinUV = minUV;
		alloc.HeightmapPatchesPtr[i].UVScale = uvScale;
		alloc.HeightmapPatchesPtr[i].SizeUV = sizeUV;
		alloc.HeightmapPatchesPtr[i].MipLevel = heightmap->GetMaxDepthLevel() - coord.Level;
		alloc.HeightmapPatchesPtr[i].TextureResolution = uint2{ texDesc.width, texDesc.height };
		alloc.HeightmapPatchesPtr[i].NormalMatrixCol0 = float4{ nodeNormalMatrix[0], 0.0 };
		alloc.HeightmapPatchesPtr[i].NormalMatrixCol1 = float4{ nodeNormalMatrix[1], 0.0 };
		alloc.HeightmapPatchesPtr[i].NormalMatrixCol2 = float4{ nodeNormalMatrix[2], 0.0 };
		alloc.HeightmapPatchesPtr[i].InverseHeightmapMatrix = inverseHeightmapMatrix;
		alloc.HeightmapPatchesPtr[i].EdgeMask = coord.edgeMask;
		alloc.HeightmapPatchesPtr[i].CellSize = heightmap->GetCellSize();
		alloc.HeightmapPatchesPtr[i].CellUVScale = heightmap->GetCellSize() / (sizeUV * heightmap->GetVirtualSize());
		alloc.HeightmapPatchesPtr[i].HeightmapTextureDI = textureView;
	}
	m_InstancesAllocBaseIdx = alloc.InstancesBaseIndex;
	m_PatchesAllocBaseIndex = alloc.PatchesBaseIndex;
}
