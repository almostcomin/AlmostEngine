#pragma once

namespace alm::gfx
{
	class LoadedTexture;
};

namespace alm::gfx
{

struct TerrainMaterialLayer
{
    std::shared_ptr<LoadedTexture> BaseColorTexture;
    std::shared_ptr<LoadedTexture> NormalTexture;
    std::shared_ptr<LoadedTexture> MetalRoughTexture;
    float3 BaseColorTint = float3(1, 1, 1);
    float  Roughness = 1.0f;
    float  Metallic = 0.0f;
    float  UVScale = 1.0f;
};

struct TerrainMaterial
{
    TerrainMaterialLayer Ground;
    TerrainMaterialLayer Slope;      // pendientes
    TerrainMaterialLayer Peak;

    float SlopeAngleStartDeg = 60.0f;
    float SlopeAngleEndDeg = 90.0f;
    float PeakHeightStart = 0.7f;
    float PeakHeightEnd = 0.85f;
    float BlendSharpness = 0.5f;

    TerrainMaterial(const std::string& name) : m_Name{ name }
    {}

private:

    std::string m_Name;
};

} // namespace alm::gfx