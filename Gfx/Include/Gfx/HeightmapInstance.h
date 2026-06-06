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

	void Update(const Camera* camera, GpuSceneBuffers* gpuSceneBuffers, GpuSceneBuffersHandle gpuBuffersHandle);

	void CollectDrawInfos(const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const;

	float GetLODDistanceFactor() const { return m_LODDistanceFactor; }
	void SetLODDistanceFactor(float f) { m_LODDistanceFactor = f; }

	uint32_t GetMaxDepthLevel() const { return m_MaxLevel; }
	void SetMaxDepthLevel(uint32_t l) { m_MaxLevel = l; }

private:

	struct QuadNodeCoord
	{
		uint32_t Level;		// 0 = root
		uint2 CellIndex;	// index of the cell inside level, [0, 2^level]

		QuadNodeCoord Child(int i) const;	// i [0,3]
		QuadNodeCoord Parent() const;

		float CellSize() const;
		float2 MinUV() const;
		float2 MaxUV() const;
		float2 CenterUV() const;
		aabox3f Bounds(float minY, float maxY) const;
	};

private:

	bool ShouldSubdivide(const QuadNodeCoord& coord, const aabox3f& worldBounds, const Camera* camera);
	void SelectLODNodes(const QuadNodeCoord& coord, const Camera* camera);

private:

	std::vector<QuadNodeCoord> m_LeafNodes;
	uint32_t m_MaxLevel = 6;

	float m_LODDistanceFactor = 1.0f;

	const SceneHeightmap* m_SceneHeightmap;

	uint32_t m_InstancesAllocBaseIdx;
	uint32_t m_PatchesAllocBaseIndex;
};

} // namespace alm::gfx