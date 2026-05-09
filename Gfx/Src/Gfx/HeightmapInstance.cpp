#include "Gfx/GfxPCH.h"
#include "Gfx/HeightmapInstance.h"

alm::gfx::HeightmapInstance::HeightmapInstance(alm::weak<Heightmap> heightmap) : m_Heightmap{ heightmap }
{
}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

