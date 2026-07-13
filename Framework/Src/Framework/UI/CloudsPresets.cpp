#include "Framework/FrameworkPCH.h"
#include "Framework/UI/CloudsPresets.h"

const char* alm::fw::CloudsPresetToString(CloudsPreset preset)
{
    switch (preset)
    {
    case CloudsPreset::ClearSky: return "Clear Sky";
    case CloudsPreset::FairWeather: return "Fair weather";
    case CloudsPreset::PartlyCloudy: return "Partly cloudy";
    case CloudsPreset::Overcast: return "Overcast";
    case CloudsPreset::SoftRain: return "Soft rain";
    case CloudsPreset::DenseStorm: return "Dense storm";
    case CloudsPreset::Fog: return "Fog";
    case CloudsPreset::Cirrus: return "Cirrus";
    case CloudsPreset::Tropical: return "Tropical";
    case CloudsPreset::Hazy: return "Hazy";
    default:
        assert(0);
        return "ERROR";
    }
}

void alm::fw::ApplyCloudsPreset(alm::gfx::CloudsRenderStage::CloudsParams& p, CloudsPreset preset)
{
    switch (preset)
    {
    case CloudsPreset::ClearSky:
        p.StratusWeight = 0.4f;  p.CumulusWeight = 0.6f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0003f; p.CloudsCoverage = 0.15f;
        p.ScatteringCoeff = 0.7f; p.AbsorptionCoeff = 0.02f;
        p.DetailScale = 12.0f;   p.DetailErosionStrength = 0.4f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::FairWeather:
        p.StratusWeight = 0.3f;  p.CumulusWeight = 0.7f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0004f; p.CloudsCoverage = 0.40f;
        p.ScatteringCoeff = 0.8f; p.AbsorptionCoeff = 0.02f;
        p.DetailScale = 8.0f;    p.DetailErosionStrength = 0.2f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::PartlyCloudy:
        p.StratusWeight = 0.4f;  p.CumulusWeight = 0.6f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0004f; p.CloudsCoverage = 0.5f;
        p.ScatteringCoeff = 0.6f; p.AbsorptionCoeff = 0.05f;
        p.DetailScale = 8.0f;    p.DetailErosionStrength = 0.25f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::Overcast:
        p.StratusWeight = 0.7f;  p.CumulusWeight = 0.3f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0005f; p.CloudsCoverage = 0.70f;
        p.ScatteringCoeff = 0.4f; p.AbsorptionCoeff = 0.20f;
        p.DetailScale = 5.0f;    p.DetailErosionStrength = 0.1f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::SoftRain:
        p.StratusWeight = 0.7f;  p.CumulusWeight = 0.2f;  p.CumulonimbusWeight = 0.2f;
        p.CloudsScale = 0.0006f; p.CloudsCoverage = 0.85f;
        p.ScatteringCoeff = 0.3f; p.AbsorptionCoeff = 0.40f;
        p.DetailScale = 4.0f;    p.DetailErosionStrength = 0.05f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::DenseStorm:
        p.StratusWeight = 0.0f;  p.CumulusWeight = 0.2f;  p.CumulonimbusWeight = 0.9f;
        p.CloudsScale = 0.0008f; p.CloudsCoverage = 0.95f;
        p.ScatteringCoeff = 0.6f; p.AbsorptionCoeff = 0.60f;
        p.DetailScale = 12.0f;   p.DetailErosionStrength = 0.5f;
        p.AmbientStrength = 0.5f;
        break;

    case CloudsPreset::Fog:
        p.StratusWeight = 1.0f;  p.CumulusWeight = 0.0f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0002f; p.CloudsCoverage = 0.95f;
        p.ScatteringCoeff = 0.5f; p.AbsorptionCoeff = 0.10f;
        p.DetailScale = 2.0f;    p.DetailErosionStrength = 0.0f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::Cirrus:
        p.StratusWeight = 1.0f;  p.CumulusWeight = 0.0f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0003f; p.CloudsCoverage = 0.30f;
        p.ScatteringCoeff = 0.3f; p.AbsorptionCoeff = 0.01f;
        p.DetailScale = 15.0f;   p.DetailErosionStrength = 0.6f;
        p.AmbientStrength = 1.f;
        break;

    case CloudsPreset::Tropical:
        p.StratusWeight = 0.0f;  p.CumulusWeight = 1.0f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0005f; p.CloudsCoverage = 0.50f;
        p.ScatteringCoeff = 0.7f; p.AbsorptionCoeff = 0.04f;
        p.DetailScale = 10.0f;   p.DetailErosionStrength = 0.3f;
        p.AmbientStrength = 1.3f;
        break;

    case CloudsPreset::Hazy:
        p.StratusWeight = 0.95f; p.CumulusWeight = 0.05f; p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0003f; p.CloudsCoverage = 0.75f;
        p.ScatteringCoeff = 0.3f; p.AbsorptionCoeff = 0.50f;
        p.DetailScale = 3.0f;    p.DetailErosionStrength = 0.0f;
        p.AmbientStrength = 1.f;
        break;

    default:
        break;
    }
}
