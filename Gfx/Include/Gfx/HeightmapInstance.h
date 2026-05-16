#pragma once

namespace alm::gfx
{

class Heightmap;

class HeightmapInstance
{
public:

	HeightmapInstance(std::weak_ptr<Heightmap> heightmap);
	~HeightmapInstance();

private:

	std::weak_ptr<Heightmap> m_Heightmap;
};

} // namespace alm::gfx