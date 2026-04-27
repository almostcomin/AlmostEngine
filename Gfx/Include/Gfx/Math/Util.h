#pragma once

#include "Core/Math/glm_config.h"

namespace alm::math
{

template<int n> 
inline uint32_t VectorToSnorm8(const glm::vec<n, float, glm::defaultp>& v)
{
	static_assert(false, "Not defined");
}

template<> 
inline uint32_t VectorToSnorm8<2>(const glm::vec2& v)
{
    float scale = 127.0f / glm::length(v);
    int x = int(v.x * scale);
    int y = int(v.y * scale);
    return (x & 0xff) | ((y & 0xff) << 8);
}

template<>
inline uint32_t VectorToSnorm8<3>(const glm::vec3& v)
{
    float scale = 127.0f / glm::length(v);
    int x = int(v.x * scale);
    int y = int(v.y * scale);
    int z = int(v.z * scale);
    return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16);
}

template<>
inline uint32_t VectorToSnorm8<4>(const glm::vec4& v)
{
    float scale = 127.0f / glm::length(v);
    int x = int(v.x * scale);
    int y = int(v.y * scale);
    int z = int(v.z * scale);
    int w = int(v.w * scale);
    return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16) | ((w & 0xff) << 24);
}

}