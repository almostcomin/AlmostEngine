#pragma once

#include "Core/Math/plane.h"
#include "Core/Math/aabox.h"

namespace st::math
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

        planes[near_plane] = st::math::plane3f{ row3 + row2 };
        planes[far_plane] = st::math::plane3f{ row3 - row2 };
        planes[left_plane] = st::math::plane3f{ row3 + row0 };
        planes[right_plane] = st::math::plane3f{ row3 - row0 };
        planes[top_plane] = st::math::plane3f{ row3 - row1 };
        planes[bottom_plane] = st::math::plane3f{ row3 + row1 };
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

    bool check(const st::math::aabox3f& b) const
    {
        for (int i = 0; i < PLANE_COUNT; ++i)
        {
            glm::vec3 p{
                planes[i].normal.x >= 0.f ? b.max.x : b.min.x,
                planes[i].normal.y >= 0.f ? b.max.y : b.min.y,
                planes[i].normal.z >= 0.f ? b.max.z : b.min.z };
            
            auto d = planes[i].distance(p);
            if (d < 0.f)
                return false;
        }
        return true;
    }
};

} // namespace st::math