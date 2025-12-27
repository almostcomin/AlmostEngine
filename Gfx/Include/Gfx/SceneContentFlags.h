#pragma once

#include <stdint.h>

namespace st::gfx
{

enum class SceneContentFlags : uint32_t
{
    None = 0,
    OpaqueMeshes = 0x01,
    AlphaTestedMeshes = 0x02,
    BlendedMeshes = 0x04,
    Lights = 0x08,
    Cameras = 0x10,
    Animations = 0x20
};
ENUM_CLASS_FLAG_OPERATORS(SceneContentFlags);

} // namespace st::gfx