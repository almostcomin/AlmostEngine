#pragma once

#include <stdint.h>

namespace alm::gfx
{

enum class SceneContentFlags : uint32_t
{
    None                = 0x00,
    OpaqueMeshes        = 0x01,
    AlphaTestedMeshes   = 0x02,
    BlendedMeshes       = 0x04,
    DirectionalLights    = 0x08,
    PointLights         = 0x10,
    SpotLights          = 0x20,
    Cameras             = 0x40,
    Animations          = 0x80
};
ENUM_CLASS_FLAG_OPERATORS(SceneContentFlags);

} // namespace st::gfx