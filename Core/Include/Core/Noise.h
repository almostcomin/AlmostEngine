#pragma once

namespace alm
{

// Gradient noise by iq (modified to be tileable)
// Returns range [-1, 1]
float GradientNoise(const float3& x, float freq);

// Tileable 3D worley noise
// Returns range [0, 1]
float WorleyNoise(const float3& uv, float freq);

// Fbm for Perlin noise based on iq's blog
// Returns range [-1, 1]
float PerlinFbm(const float3& p, float freq, int octaves);

} // namespace alm