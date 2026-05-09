#pragma once

namespace alm::gfx
{

class Heightmap;

class HeightmapInstance
{
public:

	HeightmapInstance(weak<Heightmap> heightmap);
	~HeightmapInstance();

private:

	weak<Heightmap> m_Heightmap;
};

} // namespace alm::gfx