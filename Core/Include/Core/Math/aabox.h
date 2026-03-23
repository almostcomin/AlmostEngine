#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Math/plane.h"

namespace alm::math
{

template<typename T, int n>
struct aabox
{
	static_assert(n > 1);

    using vec_t = glm::vec<n, T, glm::defaultp>;

	vec_t min;
	vec_t max;

    enum InitMode { InitEmpty };

    aabox() = default;
    aabox(InitMode)
    {
        *this = reset();
    }

    aabox(const vec_t& _min, const vec_t& _max) : min{ _min }, max{ _max }
    {}

    aabox& reset() // Set default/invalid aabox
    {
        min = vec_t{ std::numeric_limits<T>::max() };
        max = vec_t{ std::numeric_limits<T>::lowest() };
        return *this;
    }

    bool valid() const
    {
        return max.x >= min.x && max.y >= min.y && max.z >= min.z;
    }

    vec_t center() const
    {
        return (min + max) / T(2);
    }

    vec_t extents() const
    {
        return max - min;
    }

    aabox& merge(const vec_t& vec)
    {
        min = glm::min(min, vec);
        max = glm::max(max, vec);
        return *this;
    }

    aabox& merge(const aabox& other)
    {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
        return *this;
    }

    [[nodiscard]] aabox transform(const glm::mat<n + 1, n + 1, T, glm::defaultp>& mat) const
    {
        // fast method to apply an affine matrix transform to an AABB
        aabox<T, n> result;
        result.min = glm::vec3(mat[3]); // translation column
        result.max = result.min;
        for (int i = 0; i < n; i++)
        {
            auto a = glm::vec3(mat[i]) * min[i];
            auto b = glm::vec3(mat[i]) * max[i];
            result.min += glm::min(a, b);
            result.max += glm::max(a, b);
        }
        return result;
    }

    // Returns true if inside (direction of the normal) or overlaps the plane
    template<typename T>
    bool test(const plane<T, 3>& _plane) const
    {
        glm::vec3 p{
            _plane.normal.x >= 0.f ? max.x : min.x,
            _plane.normal.y >= 0.f ? max.y : min.y,
            _plane.normal.z >= 0.f ? max.z : min.z };

        return _plane.distance(p) >= 0.f;
    };

    static const aabox<T, n>& get_empty()
    {
        static aabox emptyBBox{ InitEmpty };
        return emptyBBox;
    }
};

using aabox2f = aabox<float, 2>;
using aabox3f = aabox<float, 3>;
using aabox2i = aabox<int, 2>;

} // namespace st::math