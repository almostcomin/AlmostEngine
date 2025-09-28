#pragma once

#include "Core/Math/glm_config.h"

namespace st::math
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

    aabox& reset() // Set default/invalid aabox
    {
        min = vec_t{ std::numeric_limits<T>::max() };
        max = vec_t{ std::numeric_limits<T>::lowest() };
        return *this;
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

    aabox operator * (const glm::mat<n + 1, n + 1, T, glm::defaultp>& mat) const
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

    static const aabox<T, n>& empty()
    {
        static aabox emptyBBox{ InitEmpty };
        return emptyBBox;
    }
};

using aabox3f = aabox<float, 3>;

} // namespace st::math