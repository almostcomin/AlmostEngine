#pragma once

namespace alm
{

// Gradient noise by iq (modified to be tileable)
float GradientNoise(const float3& x, float freq);

// Tileable 3D worley noise
float WorleyNoise(const float3& uv, float freq);

// Fbm for Perlin noise based on iq's blog
float PerlinFbm(const float3& p, float freq, int octaves);

} // namespace alm