#include "Gfx/GfxPCH.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/VisibleSetContext.h"
#include "Gfx/HeightmapInstance.h"

alm::gfx::SceneHeightmap::SceneHeightmap()
{
	m_RenderFlags = SceneRenderFlags::Default;
	std::ranges::fill(m_PatchMeshGpuIndices, UINT32_MAX);
}

alm::gfx::SceneHeightmap::~SceneHeightmap() = default;

const alm::aabox3f& alm::gfx::SceneHeightmap::GetBounds() const
{
	return m_Heightmap ? m_Heightmap->GetBounds() : aabox3f::get_empty();
}

void alm::gfx::SceneHeightmap::CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers,
	std::vector<RenderableDrawInfo>& out) const
{
	assert(context.HeightmapInstances);

	auto it = context.HeightmapInstances->find(this);
	assert(it != context.HeightmapInstances->end());

	it->second->CollectDrawInfos(gpuSceneBuffers, out);
}