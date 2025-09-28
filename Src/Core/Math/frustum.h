#pragma once

#include "Core/Math/plane.h"
#include "Core/Math/aabox.h"

namespace st::math
{

// six planes, normals pointing outside of the volume
struct frustum3f
{
    enum plane
    {
        near = 0,
        far,
        left,
        right,
        top,
        bottom,
        PLANE_COUNT
    };

    plane3f planes[PLANE_COUNT];

    frustum3f() = default;

    frustum3f(const glm::mat4& viewProjMat)
    {
        // GLM is column-major: viewProjMat[col][row]
        glm::vec4 row0 = glm::row(viewProjMat, 0);
        glm::vec4 row1 = glm::row(viewProjMat, 1);
        glm::vec4 row2 = glm::row(viewProjMat, 2);
        glm::vec4 row3 = glm::row(viewProjMat, 3);

        planes[near] = st::math::plane3f{ row3 + row2 };
        planes[far] = st::math::plane3f{ row3 - row2 };
        planes[left] = st::math::plane3f{ row3 + row0 };
        planes[right] = st::math::plane3f{ row3 - row0 };
        planes[top] = st::math::plane3f{ row3 - row1 };
        planes[bottom] = st::math::plane3f{ row3 + row1 };
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
                planes[i].normal.x > 0.f ? b.min.x : b.max.x,
                planes[i].normal.y > 0.f ? b.min.y : b.max.y,
                planes[i].normal.z > 0.f ? b.min.z : b.max.z };
            
            if (planes[i].distance(p) > 0.f)
                return false;
        }
        return true;
    }
};

} // namespace st::math