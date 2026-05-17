#include "Gfx/GfxPCH.h"
#include "Gfx/SceneHeigthmap.h"
#include "Gfx/Heightmap.h"

const alm::aabox3f& alm::gfx::SceneHeightmap::GetBounds() const
{
	return m_Heightmap ? m_Heightmap->GetBounds() : aabox3f::get_empty();
}

void alm::gfx::SceneHeightmap::CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers,
	std::vector<RenderableDrawInfo>& out) const
{
}