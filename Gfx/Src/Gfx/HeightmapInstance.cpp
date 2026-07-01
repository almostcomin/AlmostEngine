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

bool alm::gfx::HeightmapInstance::QuadNodeCoord::Parent(QuadNodeCoord& out_parent) const
{
	if (Level == 0)
		return false;

	out_parent = QuadNodeCoord{ Level - 1, { CellIndex.x >> 1, CellIndex.y >> 1 }};

	return true;
}

bool alm::gfx::HeightmapInstance::QuadNodeCoord::Neighbour(Axis axis, QuadNodeCoord& out_neighbour) const
{
	const int32_t cellsPerSide = 1 << Level;
	int nx = CellIndex.x;
	int ny = CellIndex.y;

	switch (axis)
	{
	case Axis::West:  nx -= 1; break;
	case Axis::East:  nx += 1; break;
	case Axis::South: ny -= 1; break;
	case Axis::North: ny += 1; break;
	default:
		assert(0);
	}
	if (nx < 0 || nx >= cellsPerSide || ny < 0 || ny >= cellsPerSide)
		return false;

	out_neighbour = QuadNodeCoord{ Level, { (uint32_t)nx, (uint32_t)ny } };
	return true;
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

	size_t size = 0;
	for (int i = 0; i < m_MaxLevel; ++i) // skip bottom level
	{
		const int32_t cellsPerSide = 1 << i;
		size += square(cellsPerSide);
	}
	m_SubdivideCache.resize(size);
	m_InQueue.resize(size);
}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

void alm::gfx::HeightmapInstance::Update(const Camera* camera, const uint2& fbSize, GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle)
{
	ZoneScoped

	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	if (m_Frozen)
	{
		FillGpuBuffers(gpuSceneBuffers, gpuBuffersHandle);
		return;
	}

	// Pre-pass
	BuildSubdivisionBFS(camera, fbSize);

	// Select leafs
	m_LeafNodes.clear();
	QuadNodeCoord root{ .Level = 0, .CellIndex{0, 0} };
	SelectLODNodes(m_LeafNodes, root, camera, fbSize);
	if (m_LeafNodes.empty())
		return;

	// Calc mesh variant
	std::unordered_set<QuadNodeCoord, QuadNodeCoordHash> leafSet;
	leafSet.reserve(m_LeafNodes.size());
	for (const auto& c : m_LeafNodes)
		leafSet.insert(c);

	for (QuadNodeCoord& node : m_LeafNodes)
	{
		Heightmap::PatchEdgeConfig edgeConfig;

		edgeConfig.North = GetEdgeMode(leafSet, node, Axis::North);
		edgeConfig.South = GetEdgeMode(leafSet, node, Axis::South);
		edgeConfig.East = GetEdgeMode(leafSet, node, Axis::East);
		edgeConfig.West = GetEdgeMode(leafSet, node, Axis::West);

		node.patchVariantIdxAndEdgeMask = Heightmap::EdgeConfigToVariantIndex(edgeConfig) & 0xff;  // 0..15

		// EdgeMask:
		{
			// Corners: node is (L, x, y)
			//			NE is (L, x+1, y+1)
			//			NW is (L, x-1, y+1)
			//			SE is (L, x+1, y-1)
			//			SW is (L, x-1, y-1)
			auto findCornerNeighbourLevel = [&](int dx, int dy) -> uint32_t
			{
				int32_t nx = (int32_t)node.CellIndex.x + dx;
				int32_t ny = (int32_t)node.CellIndex.y + dy;
				const int32_t cellsPerSide = 1 << node.Level;
				if (nx < 0 || nx >= cellsPerSide || ny < 0 || ny >= cellsPerSide)
					return UINT32_MAX;

				QuadNodeCoord cornerNode{ node.Level, { (uint32_t)nx, (uint32_t)ny } };
				if (leafSet.count(cornerNode) > 0)
					return node.Level;

				if (node.Level > 0)
				{
					QuadNodeCoord parent;
					cornerNode.Parent(parent);
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
			uint32_t edgeMask = 0;
			edgeMask |= (edgeConfig.North == Heightmap::EdgeMode::Low) ? (1u << 0) : 0u;
			edgeMask |= (edgeConfig.South == Heightmap::EdgeMode::Low) ? (1u << 1) : 0u;
			edgeMask |= (edgeConfig.East  == Heightmap::EdgeMode::Low) ? (1u << 2) : 0u;
			edgeMask |= (edgeConfig.West  == Heightmap::EdgeMode::Low) ? (1u << 3) : 0u;
			edgeMask |= (levelNE < node.Level) ? (1u << 4) : 0u;
			edgeMask |= (levelNW < node.Level) ? (1u << 5) : 0u;
			edgeMask |= (levelSE < node.Level) ? (1u << 6) : 0u;
			edgeMask |= (levelSW < node.Level) ? (1u << 7) : 0u;
			node.patchVariantIdxAndEdgeMask |= edgeMask << 16;
		}
	}	

	// We have to sort by variant so patches with the same variant (mesh) are consecutive
	// in the gpu buffers
	std::ranges::sort(m_LeafNodes, [](const QuadNodeCoord& l, const QuadNodeCoord& r) 
		{ return (l.patchVariantIdxAndEdgeMask & 0xff) < (r.patchVariantIdxAndEdgeMask & 0xff); });

	FillGpuBuffers(gpuSceneBuffers, gpuBuffersHandle);
}

void alm::gfx::HeightmapInstance::CollectDrawInfos(const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const
{
	for (int i = 0; i < m_LeafNodes.size(); ++i)
	{
		const uint32_t meshIndex = m_SceneHeightmap->GetPatchMeshGpuIndex(m_LeafNodes[i].GetMeshVariantIndex());

		out.push_back(RenderableDrawInfo{
			.MaterialDomain = MaterialDomain::Terrain,
			.CullMode = rhi::CullMode::Back,
			.BatchKey = (reinterpret_cast<uintptr_t>(this) << 16) | uintptr_t(meshIndex),
			.InstanceIdx = m_InstancesAllocBaseIdx + i,
			.MeshIndex = meshIndex,
			.MaterialIndex = gpuSceneBuffers->GetMaterialIndexFromMeshIdx(meshIndex).Index,
			.TransientBaseIndex = m_PatchesAllocBaseIndex + i,
			.IndexCount = m_SceneHeightmap->GetHeightmap()->GetPatchIndicesCount(meshIndex) });
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
	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

	float2 heightRange = heightmap->GetPatchHeightRange(node.Level, node.CellIndex);

	aabox3f localBounds = node.Bounds(heightmap->GetVirtualSize(), heightRange.x, heightRange.y);
	aabox3f worldBounds = localBounds.transform(m_SceneHeightmap->GetWorldTransform());

	return worldBounds;
}

bool alm::gfx::HeightmapInstance::ShouldSubdivideMetric(const QuadNodeCoord& coord, const Camera* camera, const uint2& fbSize)
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

bool alm::gfx::HeightmapInstance::WouldSubdivide(const QuadNodeCoord& coord) const
{
	if (coord.Level >= m_MaxLevel)
		return false;

	const uint32_t nodeIdx = GetNodeIndex(coord);
	assert(nodeIdx < m_SubdivideCache.size());

	return m_SubdivideCache[nodeIdx].subdivide;
}

void alm::gfx::HeightmapInstance::BuildSubdivisionBFS(const Camera* camera, const uint2& fbSize)
{
	ZoneScoped

	// Reset cache
	std::ranges::fill(m_SubdivideCache, SubdivideCacheElement{ false, false });
	std::ranges::fill(m_InQueue, false);

	// 1. Calc metric for all nodes in frustum and queue those that have to subdivide my metric

	std::vector<QuadNodeCoord> queue;
	queue.reserve(m_SubdivideCache.size());

	IterateQuadTreeForMetric(camera, fbSize, queue);

	// 2. BFS. Propagate neightbour subdivision
	size_t head = 0;
	while (head < queue.size())
	{
		const QuadNodeCoord& node = queue[head++];
		uint32_t nodeIndex = GetNodeIndex(node);
		m_InQueue[nodeIndex] = false;

		for (Axis axis : { Axis::North, Axis::South, Axis::East, Axis::West })
		{
			QuadNodeCoord neighbour;
			if (!node.Neighbour(axis, neighbour))
				continue;

			const uint32_t neighbourIndex = GetNodeIndex(neighbour);
			if (m_SubdivideCache[neighbourIndex].subdivide)
				continue;  // Already subdividing

			// Maybe neightbour parent is not subdivided. We have to subdivide it
			if (neighbour.Level > 0)
			{
				QuadNodeCoord parent;
				neighbour.Parent(parent);
				do
				{
					const uint32_t parentIndex = GetNodeIndex(parent);
					if (m_SubdivideCache[parentIndex].subdivide)
						break;

					m_SubdivideCache[parentIndex] = { true, true };
					if (!m_InQueue[parentIndex])
					{
						m_InQueue[parentIndex] = true;
						queue.push_back(parent);
					}
				} while (parent.Parent(parent));
			}

			// Node is at penultimate level. Its children are at max level and can't subdivide, so border children check would always be false
			if (node.Level == m_MaxLevel - 1)
				continue;

			// Any of the children in the border wants to subdivide?
			int borderChildren[2];
			switch (axis)
			{
				case Axis::West:  borderChildren[0] = 0; borderChildren[1] = 2; break;
				case Axis::East:  borderChildren[0] = 1; borderChildren[1] = 3; break;
				case Axis::South: borderChildren[0] = 0; borderChildren[1] = 1; break;
				case Axis::North: borderChildren[0] = 2; borderChildren[1] = 3; break;
			}

			for (int b : borderChildren)
			{
				QuadNodeCoord child = node.Child(b);
				const uint32_t childIndex = GetNodeIndex(child);
				if (m_SubdivideCache[childIndex].subdivide)
				{
					// We are forced to subdivide
					m_SubdivideCache[neighbourIndex] = { true, true };
					if (!m_InQueue[neighbourIndex])
					{
						m_InQueue[neighbourIndex] = true;  // in queue
						queue.push_back(neighbour);
					}
					break;
				}
			}
		}
	}
}

void alm::gfx::HeightmapInstance::IterateQuadTreeForMetric(const Camera* camera, const uint2& fbSize,
	std::vector<QuadNodeCoord>& queue)
{
	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
	const float2 uvScale = heightmap->GetUVScale();

	std::vector<QuadNodeCoord> stack;
	stack.reserve(m_SubdivideCache.size());
	
	QuadNodeCoord root{ .Level = 0, .CellIndex{0, 0} };
	stack.push_back(root);

	while (!stack.empty())
	{
		QuadNodeCoord node = stack.back();
		stack.pop_back();

		// 1. Max level
		if (node.Level == m_MaxLevel)
			continue;

		// 2. Data out of range
		{
			float2 minUV = node.MinUV() * uvScale;
			if (minUV.x >= 1.f || minUV.y >= 1.f)
				continue;
		}

		// 3. Frustum cull
		{
			aabox3f worldBounds = GetAABoxWorldSpace(node);
			if (!camera->GetFrustum().test(worldBounds))
				continue;
		}

		// 4. Metric check
		if (!ShouldSubdivideMetric(node, camera, fbSize))
		{
			continue;
		}

		// 5. Mark node and add it to the queue
		const uint32_t idx = GetNodeIndex(node);
		m_SubdivideCache[idx] = { true, true };
		m_InQueue[idx] = true;
		queue.push_back(node);

		// 6. Push children in REVERSE order so we process them in the natural order
		for (int i = 3; i >= 0; --i)
		{
			stack.push_back(node.Child(i));
		}
	}
}

void alm::gfx::HeightmapInstance::SelectLODNodes(std::vector<QuadNodeCoord>& leafNodes, const QuadNodeCoord& node,
	const Camera* camera, const uint2& fbSize)
{
	ZoneScoped

	// Check BFS
	if (WouldSubdivide(node))
	{
		for (int i = 0; i < 4; ++i)
			SelectLODNodes(leafNodes, node.Child(i), camera, fbSize);
	}
	else
	{
		leafNodes.push_back(node);
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
		QuadNodeCoord parent;
		neighbor.Parent(parent);
		if (leafSet.count(parent) > 0)
			return Heightmap::EdgeMode::Low;
	}

	// 5. No neightbour leaf found with the same level or parent's level.
	return Heightmap::EdgeMode::Normal;
}

void alm::gfx::HeightmapInstance::FillGpuBuffers(GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle)
{
	ZoneScoped

	const Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
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
		alloc.HeightmapPatchesPtr[i].EdgeMask = coord.GetEdgeMask();
		alloc.HeightmapPatchesPtr[i].CellSize = heightmap->GetCellSize();
		alloc.HeightmapPatchesPtr[i].CellUVScale = heightmap->GetCellSize() / (sizeUV * heightmap->GetVirtualSize());
		alloc.HeightmapPatchesPtr[i].HeightmapTextureDI = textureView;
	}
	m_InstancesAllocBaseIdx = alloc.InstancesBaseIndex;
	m_PatchesAllocBaseIndex = alloc.PatchesBaseIndex;
}

uint32_t alm::gfx::HeightmapInstance::GetNodeIndex(const QuadNodeCoord& coord) const
{
	const int32_t cellsPerSide = 1 << m_MaxLevel;
	const uint64_t offset = (square(cellsPerSide >> (m_MaxLevel - coord.Level)) - 1) / 3;
	const uint64_t stride = cellsPerSide >> (m_MaxLevel - coord.Level);
	
	const uint32_t nodeIdx = offset + coord.CellIndex.y * stride + coord.CellIndex.x;

	return nodeIdx;
}

alm::gfx::HeightmapInstance::QuadNodeCoord alm::gfx::HeightmapInstance::GetNodeFromIndex(uint32_t idx) const
{
	const uint32_t cellsPerSide = 1 << m_MaxLevel;
	uint32_t offset = 0;
	int foundLevel = 0;

	for (int level = 0; level <= m_MaxLevel; ++level)
	{
		const int shift = m_MaxLevel - level;
		const int32_t cells = cellsPerSide >> shift;
		const uint32_t count = square(cells); // nodos en este nivel

		if (idx >= offset && idx < offset + count)
			break;

		offset += count; // pasar al siguiente nivel
	}

	const uint32_t relIdx = idx - offset;
	const int32_t stride = cellsPerSide >> (m_MaxLevel - foundLevel);

	QuadNodeCoord coord;
	coord.Level = foundLevel;
	coord.CellIndex.x = static_cast<int32_t>(relIdx % stride);
	coord.CellIndex.y = static_cast<int32_t>(relIdx / stride);

	return coord;
}