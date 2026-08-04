#pragma once

#include "Gfx/AtmosphereConfig.h"

namespace alm::fw
{

enum class CloudsPreset
{
    ClearSky,
    FairWeather,
    PartlyCloudy,
    Overcast,
    SoftRain,
    DenseStorm,
    Fog,
    Tropical,
    Hazy,

    _Size
};

const char* CloudsPresetToString(CloudsPreset preset);
void ApplyCloudsPreset(CloudsPreset preset, alm::gfx::AtmosphereConfig& atmos);

} // namespace alm::fw