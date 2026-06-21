#pragma once

#include "Core/Math/glm_config.h"

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;

using double2 = glm::dvec2;
using double3 = glm::dvec3;
using double4 = glm::dvec4;

using int2 = glm::ivec2;
using int3 = glm::ivec3;
using int4 = glm::ivec4;

using uint2 = glm::uvec2;
using uint3 = glm::uvec3;
using uint4 = glm::uvec4;

using short2 = glm::i16vec2;
using short3 = glm::i16vec3;
using short4 = glm::i16vec4;

using ushort = uint16_t;
using ushort2 = glm::u16vec2;
using ushort3 = glm::u16vec3;
using ushort4 = glm::u16vec4;

using float3x3 = glm::fmat3x3;
using float4x4 = glm::fmat4x4;

using double3x3 = glm::dmat3x3;
using double4x4 = glm::dmat4x4;

constexpr float PI = glm::pi<float>();
constexpr float PId = glm::pi<double>();

inline const float4x4 float4x4_I{ 1.0f };

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

template<class T>
inline T square(T v)
{
    return v * v;
}

} // namespace st