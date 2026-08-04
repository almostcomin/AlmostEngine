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

void alm::fw::ApplyCloudsPreset(CloudsPreset preset, alm::gfx::AtmosphereConfig& atmos)
{
    auto& s = atmos.CloudsShape;
    auto& p = atmos.Clouds;

    switch (preset)
    {
    case CloudsPreset::ClearSky:
        s.StratusWeight = 0.3f;  s.CumulusWeight = 0.7f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.002f; s.CloudsCoverage = 0.f;
        s.DetailScale = 8.0f;    s.DetailErosionStrength = 0.1f;

        p.ScatteringCoeff = 3.f / 1000; p.AbsorptionCoeff = 0.8f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.7f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5;
        break;

    case CloudsPreset::FairWeather:
        s.StratusWeight = 0.3f;  s.CumulusWeight = 0.7f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.002f; s.CloudsCoverage = 0.35f;
        s.DetailScale = 8.0f;    s.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 3.f / 1000; p.AbsorptionCoeff = 0.8f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.7f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5;
        break;

    case CloudsPreset::PartlyCloudy:
        s.StratusWeight = 0.33f;  s.CumulusWeight = 0.52;  s.CumulonimbusWeight = 0.54f;
        s.CloudsScale = 0.006f; s.CloudsCoverage = 0.48f;
        s.DetailScale = 12.0f;    s.DetailErosionStrength = 0.25f;
        p.ScatteringCoeff = 2.9f / 1000; p.AbsorptionCoeff = 0.1f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.95f; p.PowderEdgeWidth = 0.18f;
        p.AmbientStrength = 0.5f;
        break;

    case CloudsPreset::Overcast:
        s.StratusWeight = 0.8f;  s.CumulusWeight = 0.2f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.0015f; s.CloudsCoverage = 0.75f;
        s.DetailScale = 5.0f;    s.DetailErosionStrength = 0.1f;
        p.ScatteringCoeff = 6.f / 1000; p.AbsorptionCoeff = 0.4f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.4f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.5f;
        break;

    case CloudsPreset::SoftRain:
        s.StratusWeight = 0.6f;  s.CumulusWeight = 0.4f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.0025f; s.CloudsCoverage = 0.7f;
        s.DetailScale = 8.0f;    s.DetailErosionStrength = 0.2f;
        p.ScatteringCoeff = 5.f / 1000; p.AbsorptionCoeff = 1.5f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.3f; p.PowderEdgeWidth = 0.1f;
        p.AmbientStrength = 0.4f;
        break;

    case CloudsPreset::DenseStorm:
        s.StratusWeight = 0.0f;  s.CumulusWeight = 0.1f;  s.CumulonimbusWeight = 0.9f;
        s.CloudsScale = 0.008f; s.CloudsCoverage = 0.85f;
        s.DetailScale = 15.0f;   s.DetailErosionStrength = 0.3f;
        p.ScatteringCoeff = 10.f / 1000; p.AbsorptionCoeff = 3.f / 1000;
        p.MultiScatterContribution = 0.05f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.5f; p.PowderEdgeWidth = 0.15f;
        p.AmbientStrength = 0.3f;
        break;

    case CloudsPreset::Fog:
        s.StratusWeight = 1.0f;  s.CumulusWeight = 0.0f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.001f; s.CloudsCoverage = 0.95f;
        s.DetailScale = 3.0f;    s.DetailErosionStrength = 0.05f;
        p.ScatteringCoeff = 4.f / 1000; p.AbsorptionCoeff = 0.3f / 1000;
        p.MultiScatterContribution = 0.1f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.5f; p.PowderEdgeWidth = 0.05f;
        p.AmbientStrength = 0.7f;
        break;

    case CloudsPreset::Tropical:
        s.StratusWeight = 0.0f;  s.CumulusWeight = 0.7f;  s.CumulonimbusWeight = 0.3f;
        s.CloudsScale = 0.005f; s.CloudsCoverage = 0.55f;
        s.DetailScale = 14.0f;   s.DetailErosionStrength = 0.3f;
        p.ScatteringCoeff = 6.f / 1000; p.AbsorptionCoeff = 0.6f / 1000;
        p.MultiScatterContribution = 0.15f; p.MultiScatterOcclusion = 0.5f; p.MultiScatterEccentricity = 0.5f;
        p.PhaseGForward = 0.7f; p.PhaseGBackward = -0.3f; p.MultiScatterBaseG = 0.8f;
        p.PowderStrength = 0.85f; p.PowderEdgeWidth = 0.15f;
        p.AmbientStrength = 0.6f;
        break;

    case CloudsPreset::Hazy:
        s.StratusWeight = 0.8f;  s.CumulusWeight = 0.2f;  s.CumulonimbusWeight = 0.0f;
        s.CloudsScale = 0.003f; s.CloudsCoverage = 0.6f;
        s.DetailScale = 5.0f;    s.DetailErosionStrength = 0.1f;
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
