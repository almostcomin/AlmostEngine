#pragma once

#include "Gfx/GpuSceneBuffersHandle.h"
#include "Gfx/Renderable.h"
#include "Gfx/Heightmap.h"

namespace alm::gfx
{

class SceneHeightmap;
class Camera;
class DeviceManager;

class HeightmapInstance
{
public:

	enum class Axis
	{
		North,
		South,
		East,
		West
	};

	struct QuadNodeCoord
	{
		uint32_t Level;		// 0 = root
		uint2 CellIndex;	// index of the cell inside level, [0, 2^level]

		// index of mesh variant 16 (0..15) bits
		// edge mask 8 (16..24) bits
		//		bits 0-3 edge mode:		bit0 = N Low?,  bit1 = S Low?,  bit2 = E Low?,  bit3 = W Low?
		//		bits 4-7 corner mode:	bit4 = NE Low?, bit5 = NW Low?, bit6 = SE Low?, bit7 = SW Low?
		uint32_t patchVariantIdxAndEdgeMask; 

		QuadNodeCoord Child(int i) const;	// i [0, 3]
		bool Parent(QuadNodeCoord& out_parent) const;
		bool Neighbour(Axis axis, QuadNodeCoord& out_neighbour) const;

		float SizeUV() const;
		float2 MinUV() const;
		float2 MaxUV() const;
		float2 CenterUV() const;
		aabox3f Bounds(float sizeFactor, float minY, float maxY) const;

		uint32_t GetMeshVariantIndex() const { return patchVariantIdxAndEdgeMask & 0xff; }
		uint32_t GetEdgeMask() const { return patchVariantIdxAndEdgeMask >> 16; }

		bool operator==(const QuadNodeCoord& other) const
		{
			// Ignore patchVariantIdx
			return Level == other.Level && CellIndex.x == other.CellIndex.x && CellIndex.y == other.CellIndex.y;
		}
	};

	struct QuadNodeCoordHash
	{
		size_t operator()(const QuadNodeCoord& c) const noexcept
		{
			// Simple combine, FNV-style.
			size_t h = std::hash<uint32_t>{}(c.Level);
			h ^= std::hash<uint32_t>{}(c.CellIndex.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<uint32_t>{}(c.CellIndex.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

public:

	HeightmapInstance(const SceneHeightmap* sceneHeightmap);
	~HeightmapInstance();

	void Update(const Camera* camera, const uint2& fbSize, GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle);

	void CollectDrawInfos(const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const;

	float GetLODDistanceFactor() const { return m_LODDistanceFactor; }
	void SetLODDistanceFactor(float f) { m_LODDistanceFactor = f; }

	uint32_t GetMaxDepthLevel() const { return m_MaxLevel; }
	void SetMaxDepthLevel(uint32_t l) { m_MaxLevel = l; }

	void FreezeTesselation(bool freeze) { m_Frozen = freeze; }
	std::string DumpTesselationInfo() const;

	aabox3f GetAABoxWorldSpace(const QuadNodeCoord& node) const;

	const std::vector<QuadNodeCoord>& GetLeafNodes() const { return m_LeafNodes; }

private:

	struct SubdivideCacheElement
	{
		bool tested : 1;
		bool visible : 1;
		bool subdivide : 1;
	};

private:

	bool ShouldSubdivideMetric(const QuadNodeCoord& coord, const Camera* camera, const uint2& fbSize);

	const SubdivideCacheElement* GetCachedNodeData(const QuadNodeCoord& coord) const;
	Heightmap::EdgeMode GetEdgeMode(const std::unordered_set<QuadNodeCoord, QuadNodeCoordHash>& leafSet, const QuadNodeCoord& coord, Axis axis);

	// Iterative Breadth-First search
	void BuildSubdivisionBFS(const Camera* camera, const uint2& fbSize);
	void IterateQuadTreeForMetric(const Camera* camera, const uint2& fbSize, std::vector<QuadNodeCoord>& queue);

	void SelectLODNodes(std::vector<QuadNodeCoord>& leafNodes, const QuadNodeCoord& node, const Camera* camera, const uint2& fbSize);

	void SetMeshVariantsAndEdgeMask();
	void FillGpuBuffers(GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle);

	uint32_t GetNodeIndex(const QuadNodeCoord& coord) const;
	QuadNodeCoord GetNodeFromIndex(uint32_t idx) const;

private:

	std::vector<QuadNodeCoord> m_LeafNodes;
	uint32_t m_MaxLevel = 6;

	float m_LODDistanceFactor = 12.0f;

	const SceneHeightmap* m_SceneHeightmap;

	uint32_t m_InstancesAllocBaseIdx;
	uint32_t m_PatchesAllocBaseIndex;

	std::vector<SubdivideCacheElement> m_SubdivideCache;
	std::vector<bool> m_InQueue;

	bool m_Frozen = false;
};

} // namespace alm::gfx