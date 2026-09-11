#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Math/types.h"
#include "Core/Math/aabox.h"

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

// Solves the ray vs axis-aligned box intersection using the slab method.
//
// @param rayOrigin Origin of the ray (point in space).
// @param rayDir    Direction of the ray. Does NOT need to be normalized: the
//                  returned t values are parametric along rayDir. Pass a
//                  normalized rayDir if euclidean distances are wanted.
// @param box       Axis aligned box. Must be valid (max >= min).
//
// @return std::nullopt if the ray misses the box, or if the box lies entirely
//         behind the ray origin.
//
//         A populated vec2{tNear, tFar} otherwise. The ray intersects the box
//         for rayOrigin + t * rayDir with t in [tNear, tFar]. Sign conventions:
//
//           tNear > 0 && tFar > 0 : standard case, box in front of the origin.
//           tNear < 0 && tFar > 0 : rayOrigin is inside the box. tNear is
//                                   behind the origin; clamp it to 0 when the
//                                   entry distance is wanted.
//
// @note Robust for rays parallel to any slab pair (zero direction components):
//       does not rely on IEEE inf/NaN arithmetic, so it is safe under
//       fast-math compilation flags (e.g. /fp:fast).
// @note A degenerate zero-length rayDir returns the full interval when the
//       origin is inside the box; callers are responsible for validating rayDir.

template<typename T, glm::qualifier Q>
std::optional<glm::vec<2, T, Q>> RayAABBIntersection(
    const glm::vec<3, T, Q>& rayOrigin, const glm::vec<3, T, Q>& rayDir, const aabox<T, 3>& box)
{
    glm::vec<3, T, Q> slabNear{ -std::numeric_limits<T>::max() };
    glm::vec<3, T, Q> slabFar{ std::numeric_limits<T>::max() };

    for (int axis = 0; axis < 3; ++axis)
    {
        const T d = rayDir[axis];
        if (glm::abs(d) < std::numeric_limits<T>::epsilon())
        {
            // Parallel to the slab planes: intersects only if the origin lies inside the slab
            if (rayOrigin[axis] < box.min[axis] || rayOrigin[axis] > box.max[axis])
                return std::nullopt;
            // Otherwise this axis does not constrain the interval
        }
        else
        {
            const T invD = T(1) / d;
            T t0 = (box.min[axis] - rayOrigin[axis]) * invD;
            T t1 = (box.max[axis] - rayOrigin[axis]) * invD;
            if (t0 > t1)
                std::swap(t0, t1);
            slabNear[axis] = t0;
            slabFar[axis] = t1;
        }
    }

    const T tNear = glm::max(slabNear.x, glm::max(slabNear.y, slabNear.z));
    const T tFar = glm::min(slabFar.x, glm::min(slabFar.y, slabFar.z));

    if (tNear > tFar || tFar < T(0))
        return std::nullopt;

    return glm::vec<2, T, Q>(tNear, tFar);
}

// Solves the ray vs triangle intersection using the Moller-Trumbore algorithm.
//
// @param rayOrigin Origin of the ray (point in space).
// @param rayDir    Direction of the ray. Does NOT need to be normalized: the
//                  returned t is parametric along rayDir. Pass a normalized
//                  rayDir if euclidean distances are wanted.
// @param v0, v1, v2  Triangle vertices, in any winding order.
//
// @return std::nullopt if the ray misses the triangle: ray parallel to the
//         triangle plane (or degenerate zero-area triangle), intersection
//         outside the triangle edges, or hit behind the ray origin.
//
//         A populated vec3{t, u, v} otherwise, where the intersection point is
//         v0 + u * (v1 - v0) + v * (v2 - v0), with u >= 0, v >= 0, u + v <= 1.
//
// @note Both face orientations are accepted (backface hits included), so the
//       test is two-sided. The geometric face normal is cross(v1 - v0, v2 - v0),
//       oriented according to the winding order; normalize it when needed.
// @note Hits exactly at the ray origin (t == 0) are rejected to avoid self
//       intersections when casting rays from surfaces. This does not affect
//       picking from free origins (e.g. mouse rays from a camera).
// @note Does not rely on IEEE inf/NaN arithmetic; safe under fast-math
//       compilation flags (e.g. /fp:fast).

template<typename T, glm::qualifier Q>
std::optional<glm::vec<3, T, Q>> RayTriangleIntersection(
    const glm::vec<3, T, Q>& rayOrigin, const glm::vec<3, T, Q>& rayDir,
    const glm::vec<3, T, Q>& v0, const glm::vec<3, T, Q>& v1, const glm::vec<3, T, Q>& v2)
{
    const glm::vec<3, T, Q> e1 = v1 - v0;
    const glm::vec<3, T, Q> e2 = v2 - v0;
    const glm::vec<3, T, Q> p = glm::cross(rayDir, e2);
    const T det = glm::dot(e1, p);
    if (glm::abs(det) < std::numeric_limits<T>::epsilon())
        return std::nullopt; // Parallel to the triangle plane (also rejects degenerate zero-area triangles)

    const T invDet = T(1) / det;
    const glm::vec<3, T, Q> tvec = rayOrigin - v0;
    const T u = glm::dot(tvec, p) * invDet;
    if (u < T(0) || u > T(1))
        return std::nullopt;

    const glm::vec<3, T, Q> q = glm::cross(tvec, e1);
    const T v = glm::dot(rayDir, q) * invDet;
    if (v < T(0) || u + v > T(1))
        return std::nullopt;

    const T t = glm::dot(e2, q) * invDet;
    if (t <= T(0))
        return std::nullopt; // Hit behind the ray origin

    return glm::vec<3, T, Q>{ t, u, v };
}

} // namespace st