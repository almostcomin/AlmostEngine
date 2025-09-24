#pragma once

#include "Core/Math/plane.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace st::math
{

// six planes, normals pointing outside of the volume
struct frustum
{
    enum plane
    {
        near = 0,
        far,
        left,
        right,
        top,
        bottom,
        _COUNT
    };

    plane3f planes[(int)plane::_COUNT];

    frustum() = default;

    frustum(const glm::mat4& vp)
    {
        // GLM es column-major: vp[col][row]
        glm::vec4 row0 = glm::row(vp, 0);
        glm::vec4 row1 = glm::row(vp, 1);
        glm::vec4 row2 = glm::row(vp, 2);
        glm::vec4 row3 = glm::row(vp, 3);

        planes[plane::near] = st::math::plane3f{ row3 + row2 };
        planes[plane::far] = st::math::plane3f{ row3 - row2 };
        planes[plane::left] = st::math::plane3f{ row3 + row0 };
        planes[plane::right] = st::math::plane3f{ row3 - row0 };
        planes[plane::top] = st::math::plane3f{ row3 - row1 };
        planes[plane::bottom] = st::math::plane3f{ row3 + row1 };

    }
};

} // namespace st::math