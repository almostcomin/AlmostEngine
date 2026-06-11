#pragma once

namespace alm::gfx
{

class IHeightmapSource;
class DeviceManager;
class Mesh;
struct TerrainMaterial;

class Heightmap
{
public:

	static constexpr uint32_t kPatchMeshVariantsCount = 16;

	enum class EdgeMode
	{
		Normal,
		Low
	};

	struct PatchEdgeConfig
	{
		EdgeMode North;
		EdgeMode South;
		EdgeMode East;
		EdgeMode West;
	};

public:

	Heightmap(DeviceManager* deviceManager);
	~Heightmap();

	void Init(std::shared_ptr<IHeightmapSource> source,
		std::shared_ptr<TerrainMaterial> material,
		const uint2& textureResolution,
		uint32_t patchResolution = 64,
		const float2& uvScale = { 1.f, 1.f },
		const float2& uvOffset = { 0.f, 0.f });

	float Sample(const float2& uv) const;

	const std::shared_ptr<IHeightmapSource>& GetSource() const { return m_Source; }
	const aabox3f& GetBounds() const { return m_Bounds; }

	uint32_t GetMaxDepthLevel() const;

	void SetTextureResolution(const uint2& textureResolution);
	const uint2& GetTextureResolution() const { return m_TextureResolution; }

	rhi::TextureHandle GetHeightsTexture() const { return m_HeightsTexture.get_weak(); }
	
	alm::weak<Mesh> GetPatchMesh(uint32_t variantIdx) const { return m_PatchMeshVariants[variantIdx].get_weak(); }

	std::shared_ptr<TerrainMaterial> GetMaterial() const { return m_Material; }

	void RefreshHeightsTexture();
	uint32_t GetPatchIndicesCount(uint32_t variantIdx) const;

	// 0..15
	static uint32_t EdgeConfigToVariantIndex(const PatchEdgeConfig& config);

private:

	void ComputeBounds();
	rhi::TextureOwner BuildTexture();
	void BuildPatchVariants();

	std::shared_ptr<alm::rhi::BufferOwner> CreateIndexBufferVariant(const PatchEdgeConfig& edgeConfig, const bool indices32Bits) const;

	std::shared_ptr<IHeightmapSource> m_Source;

	float2 m_uvScale;
	float2 m_uvOffset;
	uint32_t m_PatchResolution;

	aabox3f m_Bounds;

	uint2 m_TextureResolution;

	std::array<alm::unique<Mesh>, 16> m_PatchMeshVariants;

	rhi::TextureOwner m_HeightsTexture;
	std::shared_ptr<TerrainMaterial> m_Material;

	DeviceManager* m_DeviceManager;
};

} // namespace alm::gfx