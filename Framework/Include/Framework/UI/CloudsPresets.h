#pragma once

#include "Gfx/RenderStages/CloudsRenderStage.h"

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
    Cirrus,
    Tropical,
    Hazy,

    _Size
};

const char* CloudsPresetToString(CloudsPreset preset);
void ApplyCloudsPreset(alm::gfx::CloudsRenderStage::CloudsParams& p, CloudsPreset preset);

} // namespace alm::fw