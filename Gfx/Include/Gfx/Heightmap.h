#pragma once

namespace alm::gfx
{

class IHeightmapSource;

class Heightmap
{
public:

	Heightmap(
		std::shared_ptr<IHeightmapSource> source,
		const float2& uvScale = { 1.f, 1.f },
		const float2& uvOffset = { 0.f, 0.f },
		float heightScale = 1.f,
		float heightOffset = 0.f);
	~Heightmap();

	float Sample(const float2& uv) const;
	const math::aabox3f& GetBounds() const { return m_Bounds; }

private:

	void ComputeBounds();

	std::shared_ptr<IHeightmapSource> m_Source;

	float2 m_uvScale;
	float2 m_uvOffset;
	float m_HeightScale;
	float m_HeightOffset;

	math::aabox3f m_Bounds;
};

} // namespace alm::gfx