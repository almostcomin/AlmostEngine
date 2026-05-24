#pragma once

namespace alm::gfx
{

class IHeightmapSource;
class DeviceManager;
class Mesh;

class Heightmap
{
public:

	Heightmap(
		DeviceManager* deviceManager,
		std::shared_ptr<IHeightmapSource> source,
		const float2& uvScale = { 1.f, 1.f },
		const float2& uvOffset = { 0.f, 0.f },
		uint32_t patchResolution = 64);

	~Heightmap();

	float Sample(const float2& uv) const;

	const std::shared_ptr<IHeightmapSource>& GetSource() const { return m_Source; }
	const aabox3f& GetBounds() const { return m_Bounds; }

	rhi::TextureHandle GetHeightsTexture() const { return m_HeightsTexture.get_weak(); }
	alm::weak<Mesh> GetPatchMesh() { return m_PatchMesh.get_weak(); }

	uint32_t GetPatchIndicesCount() const;

private:

	void ComputeBounds();
	void BuildTexture();
	void BuildPatch();

	std::shared_ptr<IHeightmapSource> m_Source;

	float2 m_uvScale;
	float2 m_uvOffset;
	const uint32_t m_PatchResolution;

	aabox3f m_Bounds;

	alm::unique<Mesh> m_PatchMesh;
	rhi::TextureOwner m_HeightsTexture;

	DeviceManager* m_DeviceManager;
};

} // namespace alm::gfx