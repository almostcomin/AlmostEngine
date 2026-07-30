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
        p.StratusWeight = 0.3f;  p.CumulusWeight = 0.7f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.002f; p.CloudsCoverage = 0.f;
        p.DetailScale = 8.0f;    p.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 3.f / 1000; p.AbsorptionCoeff = 0.8f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.7f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5;
        break;

    case CloudsPreset::FairWeather:
        p.StratusWeight = 0.3f;  p.CumulusWeight = 0.7f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.002f; p.CloudsCoverage = 0.35f;
        p.DetailScale = 8.0f;    p.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 3.f / 1000; p.AbsorptionCoeff = 0.8f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.7f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5;
        break;

    case CloudsPreset::PartlyCloudy:
        p.StratusWeight = 0.33f;  p.CumulusWeight = 0.52;  p.CumulonimbusWeight = 0.54f;
        p.CloudsScale = 0.006f; p.CloudsCoverage = 0.48f;
        p.DetailScale = 12.0f;    p.DetailErosionStrength = 0.25f;
        p.ScatteringCoeff = 2.9f / 1000; p.AbsorptionCoeff = 0.1f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.95f; p.PowderEdgeWidth = 0.18f;
        p.AmbientStrength = 0.5f;
        break;

    case CloudsPreset::Overcast:
        p.StratusWeight = 0.8f;  p.CumulusWeight = 0.2f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0015f; p.CloudsCoverage = 0.75f;
        p.DetailScale = 5.0f;    p.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 6.f / 1000; p.AbsorptionCoeff = 0.4f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.4f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5f;
        break;

    case CloudsPreset::SoftRain:
        p.StratusWeight = 0.6f;  p.CumulusWeight = 0.4f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.0025f; p.CloudsCoverage = 0.7f;
        p.DetailScale = 8.0f;    p.DetailErosionStrength = 0.2f;
        p.ScatteringCoeff = 5.f / 1000; p.AbsorptionCoeff = 1.5f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.3f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.4f;
        break;

    case CloudsPreset::DenseStorm:
        p.StratusWeight = 0.0f;  p.CumulusWeight = 0.1f;  p.CumulonimbusWeight = 0.9f;
        p.CloudsScale = 0.008f; p.CloudsCoverage = 0.85f;
        p.DetailScale = 15.0f;   p.DetailErosionStrength = 0.3f;
        p.ScatteringCoeff = 10.f / 1000; p.AbsorptionCoeff = 3.f / 1000;
        p.MultiScatterContribution = 0.05f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.5f; p.PowderEdgeWidth = 0.15f;
        p.AmbientStrength = 0.3f;
        break;

    case CloudsPreset::Fog:
        p.StratusWeight = 1.0f;  p.CumulusWeight = 0.0f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.001f; p.CloudsCoverage = 0.95f;
        p.DetailScale = 3.0f;    p.DetailErosionStrength = 0.05f;
        p.ScatteringCoeff = 4.f / 1000; p.AbsorptionCoeff = 0.3f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.5f; p.PowderEdgeWidth = 0.05f;
        p.AmbientStrength = 0.7f;
        break;

    case CloudsPreset::Tropical:
        p.StratusWeight = 0.0f;  p.CumulusWeight = 0.7f;  p.CumulonimbusWeight = 0.3f;
        p.CloudsScale = 0.005f; p.CloudsCoverage = 0.55f;
        p.DetailScale = 14.0f;   p.DetailErosionStrength = 0.3f;
        p.ScatteringCoeff = 6.f / 1000; p.AbsorptionCoeff = 0.6f / 1000;
        p.MultiScatterContribution = 0.15f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.85f; p.PowderEdgeWidth = 0.15f;
        p.AmbientStrength = 0.6f;
        break;

    case CloudsPreset::Hazy:
        p.StratusWeight = 0.8f;  p.CumulusWeight = 0.2f;  p.CumulonimbusWeight = 0.0f;
        p.CloudsScale = 0.003f; p.CloudsCoverage = 0.6f;
        p.DetailScale = 5.0f;    p.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 2.f / 1000; p.AbsorptionCoeff = 1.5f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.4f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5f;
        break;

    default:
        break;
    }
}
