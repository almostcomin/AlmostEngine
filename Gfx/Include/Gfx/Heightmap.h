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
		uint32_t patchResolution = 64);

	const std::shared_ptr<IHeightmapSource>& GetSource() const { return m_Source; }
	const aabox3f& GetBounds() const { return m_Bounds; }

	uint32_t GetMaxDepthLevel() const { return m_MaxDepthLevel; }

	void SetTextureResolution(const uint2& textureResolution);
	const uint2& GetTextureResolution() const { return m_TextureResolution; }

	rhi::TextureHandle GetHeightsTexture() const { return m_HeightsTexture.get_weak(); }
	
	alm::weak<Mesh> GetPatchMesh(uint32_t variantIdx) const { return m_PatchMeshVariants[variantIdx].get_weak(); }

	std::shared_ptr<TerrainMaterial> GetMaterial() const { return m_Material; }

	void RefreshHeightsTexture();
	uint32_t GetPatchIndicesCount(uint32_t variantIdx) const;
	uint32_t GetPatchResolution() const { return m_PatchResolution; }
	
	float2 GetActualSize() const { return m_ActualSize; }

	// Virtual size/count takes into consideration that the size of the heightmap (the quadtree) needs to be power of two. 
	// So it is always equal of higher that the data size
	float GetVirtualSize() const { return m_VirtualSize; }

	// Relation between virtual/data size
	float2 GetUVScale() const;
	// Returns the cell size, size of a patch quad
	float GetCellSize() const;

	float GetPatchErrorValue(uint32_t level, const uint2& c) const;
	const float2& GetPatchHeightRange(uint32_t level, const uint2& c) const;

	uint32_t GetBottomLevelPatchesCount() const { return m_BottomLevelPatchesCount; }

	// 0..15
	static uint32_t EdgeConfigToVariantIndex(const PatchEdgeConfig& config);

private:

	float GetHeight(const uint2& c) const;

	// Number of virtual cells.
	uint32_t GetVirtualCellsCount() const;

	void ComputeBounds();
	rhi::TextureOwner BuildTexture();
	
	void BuildPatchVariants();
	std::shared_ptr<alm::rhi::BufferOwner> CreateIndexBufferVariant(const PatchEdgeConfig& edgeConfig, const bool indices32Bits) const;

	void BuildErrorPyramid();
	void CalcHeightRanges();

	uint32_t GetNodeIndex(uint32_t level, const uint2& coords) const;

private:

	std::shared_ptr<IHeightmapSource> m_Source;
	
	uint32_t m_PatchResolution;	// Geometry size (per axis), power of two (tipically 32 or 64).
	uint32_t m_BottomLevelPatchesCount;
	uint32_t m_MaxDepthLevel;

	float2 m_ActualSize;		// Size of the heightmap in x,z
	// Virtual means that takes into account that the source data might not be multiple of two.
	// So virtual size is the size of the heightmap including the cells that fall outside the source data.
	float m_VirtualSize;

	std::vector<float> m_ErrorPyramid;
	std::vector<float2> m_HeightRange;

	// Bounds of the full heightmap, not 
	aabox3f m_Bounds;

	rhi::TextureOwner m_HeightsTexture;
	uint2 m_TextureResolution;

	std::array<alm::unique<Mesh>, 16> m_PatchMeshVariants;
	std::shared_ptr<TerrainMaterial> m_Material;

	DeviceManager* m_DeviceManager;
};

} // namespace alm::gfx