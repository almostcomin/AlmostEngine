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
    TerrainMaterialLayer Slope;
    TerrainMaterialLayer Peak;

    // Groound -> Peak transition
    float HeightTransitionStart = 0.7f;
    float HeightTransitionEnd = 0.85f;

    // Ground/Peak -> Slope transition
    float SlopeAngleStartDeg = 45.f;
    float SlopeAngleEndDeg = 60.f;

    float SlopeBlendSharpness = 1.f;

    TerrainMaterial(const std::string& name) : m_Name{ name }
    {}

private:

    std::string m_Name;
};

} // namespace alm::gfx