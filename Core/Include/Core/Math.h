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

template<class T>
inline T square(T v)
{
    return v * v;
}

template<class T>
inline T saturate(T v)
{
    return std::clamp<T>(v, (T)0, (T)1);
}

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

// Solves |rayOrigin + t * rayDir - sphereCenter|^2 = sphereRadius^2 for t.
//
// @param rayOrigin    Origin of the ray (point in space).
// @param rayDir       Direction of the ray. MUST be normalized (unit length); otherwise
//                     the returned t values are not in the same units as rayDir and
//                     the quadratic coefficients below are wrong.
// @param sphereCenter Center of the sphere.
// @param sphereRadius Radius of the sphere. Must be non-negative.
//
// @return std::nullopt if the ray does not intersect the sphere at all
//         (discriminant < 0). The 3D points at any t are meaningless in this case.
//
//         A populated vec2{tNear, tFar} otherwise. The intersection points are
//         rayOrigin + t * rayDir for t in {tNear, tFar}. Both t values are valid
//         mathematical solutions; they may be negative, in which case the
//         corresponding intersection point lies BEHIND the ray origin (opposite
//         to rayDir). Sign conventions:
//
//           ret.x < 0 && ret.y < 0  : sphere entirely behind the ray origin.
//                                      Both intersection points are behind.
//           ret.x < 0 && ret.y > 0  : rayOrigin is inside the sphere. tNear is
//                                      behind (entry point behind origin),
//                                      tFar is in front (exit point).
//           ret.x > 0 && ret.y > 0  : sphere entirely in front. Standard near/far.
//           ret.x == 0 || ret.y == 0: rayOrigin lies exactly on the sphere.
//
// @note The function does not classify "in front of / behind" for the caller.
//       It is the caller's responsibility to inspect t.x / t.y and decide which
//       points are useful (e.g. clamp tNear to 0 when the origin is inside).

template<typename T, glm::qualifier Q>
std::optional<glm::vec<2, T, Q>> RaySphereIntersection(
    const glm::vec<3, T, Q>& rayOrigin, const glm::vec<3, T, Q>& rayDir, const glm::vec<3, T, Q>& sphereCenter, T sphereRadius)
{
    glm::vec<3, T, Q> oc = rayOrigin - sphereCenter;
    T b = dot(oc, rayDir);
    T c = dot(oc, oc) - square(sphereRadius);
    T disc = b * b - c;
    if (disc < T(0))
        return std::nullopt;

    T q = (b > T(0)) ? (-b - sqrt(disc)) : (-b + sqrt(disc)); // max-magnitude root

    T root1 = q;
    T root2 = c / q;
    if (root1 > root2)
        std::swap(root1, root2);

    return glm::vec<2, T, Q>(root1, root2);
}

} // namespace st