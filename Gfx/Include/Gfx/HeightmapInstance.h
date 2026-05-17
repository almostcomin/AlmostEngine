#pragma once

namespace alm::gfx
{

class SceneHeightmap;
class Camera;

class HeightmapInstance
{
public:

	HeightmapInstance(const SceneHeightmap* sceneHeightmap);
	~HeightmapInstance();

	void Update(const Camera* camera, rhi::ICommandList* commandList);

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
	void SelectLODNodes(const QuadNodeCoord& coord, const Camera* camera, std::vector<QuadNodeCoord>& out_leafNodes);

private:

	std::vector<QuadNodeCoord> m_LeafNodes;
	uint32_t m_MaxLevel;

	float m_LODDistanceFactor = 1.f;

	const SceneHeightmap* m_SceneHeightmap;
};

} // namespace alm::gfx