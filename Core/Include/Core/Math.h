#pragma once

#include "Core/Math/glm_config.h"

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;

using int2 = glm::ivec2;
using int3 = glm::ivec3;
using int4 = glm::ivec4;

using uint2 = glm::uvec2;
using uint3 = glm::uvec3;
using uint4 = glm::uvec4;

using float3x3 = glm::fmat3x3;
using float4x4 = glm::fmat4x4;

constexpr float PI = glm::pi<float>();

namespace alm
{

inline float3 ElevationAzimuthRadToDir(float elevationRad, float azimuthRad)
{
    float x = glm::cos(elevationRad) * glm::sin(azimuthRad);
    float y = glm::sin(elevationRad);
    float z = glm::cos(elevationRad) * glm::cos(azimuthRad);

    return glm::normalize(-float3{ x, y, z });
}

inline std::pair<float, float> DirToElevationAzimuthRad(const float3& dir)
{
    float3 normInvDir = -glm::normalize(dir);

    float elevation = glm::asin(normInvDir.y);
    float azimuth = glm::atan(normInvDir.x, normInvDir.z);

    return { elevation, azimuth };
}

} // namespace st