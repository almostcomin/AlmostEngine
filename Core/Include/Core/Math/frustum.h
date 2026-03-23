#pragma once

#include "Core/Math/plane.h"
#include "Core/Math/aabox.h"

namespace alm::math
{

// six planes, normals pointing outside of the volume
struct frustum3f
{
    enum
    {
        near_plane = 0,
        far_plane,
        left_plane,
        right_plane,
        top_plane,
        bottom_plane,
        PLANE_COUNT
    };

    plane3f planes[PLANE_COUNT];

    frustum3f() = default;

    frustum3f(const glm::mat4& mat)
    {
        // GLM is column-major: viewProjMat[col][row]
        // Plane normals pointing inside the volume
        glm::vec4 row0 = glm::row(mat, 0);
        glm::vec4 row1 = glm::row(mat, 1);
        glm::vec4 row2 = glm::row(mat, 2);
        glm::vec4 row3 = glm::row(mat, 3);

        planes[near_plane] = alm::math::plane3f{ row3 + row2 };
        planes[far_plane] = alm::math::plane3f{ row3 - row2 };
        planes[left_plane] = alm::math::plane3f{ row3 + row0 };
        planes[right_plane] = alm::math::plane3f{ row3 - row0 };
        planes[top_plane] = alm::math::plane3f{ row3 - row1 };
        planes[bottom_plane] = alm::math::plane3f{ row3 + row1 };
    }

    std::span<const plane3f, PLANE_COUNT> get_planes() const
    {
        return std::span<const plane3f, PLANE_COUNT>(planes);
    }

    bool check(const glm::vec3& p) const
    {
        for (int i = 0; i < PLANE_COUNT; ++i)
        {
            if (planes[i].distance(p) > 0.f)
                return false;
        }
        return true;
    }

    bool test(const alm::math::aabox3f& b) const
    {
        for (int i = 0; i < PLANE_COUNT; ++i)
        {
            if(!b.test(planes[i]))
                return false;
        }
        return true;
    }
};

} // namespace st::math