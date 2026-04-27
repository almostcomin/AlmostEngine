#pragma once

#include <stdint.h>

namespace alm::gfx
{

enum class SceneContentType
{
    Meshes              = 0,
    ShadowCasters       = 1,
    DirectionalLights   = 2,
    PointLights         = 3,
    SpotLights          = 4,
    Cameras             = 5,
    Animations          = 6,
    _Size
};

enum class SceneContentFlags : uint32_t
{
    None                = 0x00,
    Meshes              = 0x01 << (int)SceneContentType::Meshes,
    ShadowCasters       = 0x01 << (int)SceneContentType::ShadowCasters,
    DirectionalLights   = 0x01 << (int)SceneContentType::DirectionalLights,
    PointLights         = 0x01 << (int)SceneContentType::PointLights,
    SpotLights          = 0x01 << (int)SceneContentType::SpotLights,
    Cameras             = 0x01 << (int)SceneContentType::Cameras,
    Animations          = 0x01 << (int)SceneContentType::Animations,

    Lights              = DirectionalLights | PointLights | SpotLights
};
ENUM_CLASS_FLAG_OPERATORS(SceneContentFlags);

constexpr SceneContentFlags ToFlag(SceneContentType t)
{
    return (SceneContentFlags)(1u << (int)t);
}

constexpr int ToIndex(SceneContentType t)
{
    return (int)t;
}

constexpr bool HasBoundsCategory(SceneContentType t)
{
    switch (t) {
    case SceneContentType::Meshes:
    case SceneContentType::ShadowCasters:
    case SceneContentType::PointLights:
    case SceneContentType::SpotLights:
        return true;
    case SceneContentType::DirectionalLights:
    case SceneContentType::Cameras:
    case SceneContentType::Animations:
        return false;
    }
    return false;
}

} // namespace st::gfx