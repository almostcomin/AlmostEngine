#include "Gfx/GfxPCH.h"
#include "Gfx/HeightmapInstance.h"

alm::gfx::HeightmapInstance::HeightmapInstance(std::weak_ptr<Heightmap> heightmap) : m_Heightmap{ heightmap }
{
}

alm::gfx::HeightmapInstance::~HeightmapInstance()
{}

