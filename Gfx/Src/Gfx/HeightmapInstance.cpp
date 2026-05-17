#include "Gfx/GfxPCH.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/SceneHeigthmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/Camera.h"

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

alm::gfx::HeightmapInstance::HeightmapInstance(const SceneHeightmap* sceneHeightmap) : 
	m_MaxLevel{ 4 }, m_SceneHeightmap { sceneHeightmap }
{}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

void alm::gfx::HeightmapInstance::Update(const Camera* camera, rhi::ICommandList* commandList)
{
	m_LeafNodes.clear();
	QuadNodeCoord root{
		.Level = 0, .CellIndex{0, 0} };

	SelectLODNodes(root, camera, m_LeafNodes);
}

bool alm::gfx::HeightmapInstance::ShouldSubdivide(const QuadNodeCoord& coord, const aabox3f& worldBounds, const Camera* camera)
{	
	const float3 worldCenter = worldBounds.center();
	const float dist = glm::distance(camera->GetPosition(), worldCenter);
	const float3 d = worldBounds.diagonal();
	const float2 dXZ = float2{ d.x, d.z };
	const float size = dXZ.length();

	return dist < size * m_LODDistanceFactor;
}

void alm::gfx::HeightmapInstance::SelectLODNodes(const QuadNodeCoord& coord, const Camera* camera,
	std::vector<QuadNodeCoord>& out_leafNodes)
{
	// Data out of range (not tileable only)
	const auto& dataSource = m_SceneHeightmap->GetHeightmap()->GetSource();
	if (!dataSource->IsTileable())
	{
		float2 dataNormSize = dataSource->GetNormalizedSize();
		float2 minUV = coord.MinUV();
		if (minUV.x > dataNormSize.x || minUV.y > dataNormSize.y)
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
		for (int i = 0; i < 3; ++i)
		{
			SelectLODNodes(coord.Child(i), camera, out_leafNodes);
		}
	}
	else
	{
		out_leafNodes.push_back(coord);
	}
}