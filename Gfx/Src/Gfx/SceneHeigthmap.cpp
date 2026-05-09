#include "Gfx/GfxPCH.h"
#include "Gfx/SceneHeigthmap.h"
#include "Gfx/Heightmap.h"

const alm::math::aabox3f& alm::gfx::SceneHeightmap::GetBounds() const
{
	return m_Heightmap ? m_Heightmap->GetBounds() : math::aabox3f::get_empty();
}