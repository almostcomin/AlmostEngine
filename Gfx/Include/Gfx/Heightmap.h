#pragma once

namespace alm::gfx
{

class IHeightmapSource;
class DeviceManager;

class Heightmap
{
public:

	Heightmap(
		DeviceManager* deviceManager,
		std::shared_ptr<IHeightmapSource> source,
		const float2& uvScale = { 1.f, 1.f },
		const float2& uvOffset = { 0.f, 0.f },
		float heightScale = 1.f,
		float heightOffset = 0.f);

	~Heightmap();

	float Sample(const float2& uv) const;

	const std::shared_ptr<IHeightmapSource>& GetSource() const { return m_Source; }
	const aabox3f& GetBounds() const { return m_Bounds; }
	rhi::TextureHandle GetHeightsTexture() const { return m_HeightsTexture.get_weak(); }

private:

	void ComputeBounds();
	void BuildTexture();

	std::shared_ptr<IHeightmapSource> m_Source;

	float2 m_uvScale;
	float2 m_uvOffset;
	float m_HeightScale;
	float m_HeightOffset;

	aabox3f m_Bounds;

	rhi::TextureOwner m_HeightsTexture;

	DeviceManager* m_DeviceManager;
};

} // namespace alm::gfx