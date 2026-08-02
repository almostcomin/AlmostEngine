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
void ApplyCloudsPreset(alm::gfx::AtmosphereConfig::CloudsParams& p, CloudsPreset preset);

} // namespace alm::fw