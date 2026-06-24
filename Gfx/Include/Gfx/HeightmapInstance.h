#pragma once

#include "Gfx/GpuSceneBuffersHandle.h"
#include "Gfx/Renderable.h"

namespace alm::gfx
{

class SceneHeightmap;
class Camera;
class DeviceManager;

class HeightmapInstance
{
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
	std::vector<alm::aabox3f> CollectAABBoxes() const;

private:

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
		uint32_t patchVariantIdx; // index of mesh variant [0, 15]
		uint32_t edgeMask; 

		QuadNodeCoord Child(int i) const;	// i [0, 3]
		QuadNodeCoord Parent() const;

		float SizeUV() const;
		float2 MinUV() const;
		float2 MaxUV() const;
		float2 CenterUV() const;
		aabox3f Bounds(float sizeFactor, float minY, float maxY) const;

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

private:

	bool ShouldSubdivide(const QuadNodeCoord& coord, const aabox3f& worldBounds, const Camera* camera, const uint2& fbSize);
	bool NeighborWouldForceSubdivide(const QuadNodeCoord& coord, Axis axis, const Camera* camera, const uint2& fbSize);
	uint32_t FindNeighbourLevel(const std::unordered_set<QuadNodeCoord, QuadNodeCoordHash>& leafSet, const QuadNodeCoord& coord, Axis axis);

	void SelectLODNodes(std::vector<QuadNodeCoord>& leafNodes, const QuadNodeCoord& coord, const Camera* camera, const uint2& fbSize);

	void FillGpuBuffers(GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle);

private:

	std::vector<QuadNodeCoord> m_LeafNodes;
	uint32_t m_MaxLevel = 6;

	float m_LODDistanceFactor = 1.0f;

	const SceneHeightmap* m_SceneHeightmap;

	uint32_t m_InstancesAllocBaseIdx;
	uint32_t m_PatchesAllocBaseIndex;

	bool m_Frozen = false;
};

} // namespace alm::gfx