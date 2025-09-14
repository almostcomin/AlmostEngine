#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/common.hpp>

namespace st::math
{

template<typename T, int n>
struct aabox
{
	static_assert(n > 1);

    using vec_t = glm::vec<n, T, glm::defaultp>;

	vec_t min;
	vec_t max;

    aabox& Reset() // Set default/invalid aabox
    {
        min = vec_t{ std::numeric_limits<T>::max() };
        max = vec_t{ std::numeric_limits<T>::lowest() };
        return *this;
    }

    aabox& Merge(const vec_t& vec)
    {
        min = glm::min(min, vec);
        max = glm::max(max, vec);
        return *this;
    }
};

using aabox3f = aabox<float, 3>;

}