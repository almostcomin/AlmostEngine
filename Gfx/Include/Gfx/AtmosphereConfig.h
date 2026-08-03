#pragma once

#include "RHI/ShaderViews.h"

namespace alm::gfx
{

class DeviceManager;

class AtmosphereConfig
{
public:
    // ------------------------------------------------------------
    // Reference constants (unscaled base values)
    // ------------------------------------------------------------
    static constexpr float kEarthRadiusRef = 6360000.f;

    static constexpr float kAtmosHeightRef = 60000.f;
    static constexpr float3 kRefRayleighWaveLengths = { 680, 530.f, 440.f };
    static constexpr float kRefRayleighScaleHeight = 8000.f;
    static constexpr float kMieBase = 21e-6f;
    static constexpr float kRefMieScaleHeight = 1200.f;

    static constexpr float kCloudsLayerMinRef = 1500.f;
    static constexpr float kCloudsLayerMaxRef = 12000.f;
    static constexpr float kCloudsFadeDistRef = 48000.f;

    // ------------------------------------------------------------
    // Sub-structures (all parameters are public and modifiable)
    // ------------------------------------------------------------
    struct AmbientParams
    {
        float3 SkyColor = { 0.17f, 0.37f, 0.65f };
        float3 GroundColor = { 0.62f, 0.58f, 0.55f };
        float  Intensity = 0.22f;
    };

    struct SunParams
    {
        float  ElevationDeg = 60.f;
        float  AzimuthDeg = -135.f;
        float  Irradiance = 1.f;
        float  AngularSizeDeg = 0.53f;
        float3 Color = { 1.f, 1.f, 1.f };
    };

    struct CloudsParams
    {
        // Weights and coverage (dimensionless)
        float StratusWeight = 0.3f;
        float CumulusWeight = 0.7f;
        float CumulonimbusWeight = 0.f;
        float CloudsCoverage = 0.35f;

        // ---- Spatial frequencies (1/m) ----
        // MUST be divided by EarthScaleFactor at shader binding time.
        float CloudsScale = 0.004f;
        float DetailScale = 80.f;

        // ---- Extinction/scattering densities (1/m) ----
        // MUST also be divided by EarthScaleFactor at binding time.
        float AbsorptionCoeff = 0.8f / 1000.f;   // 1/m
        float ScatteringCoeff = 3.f / 1000.f;    // 1/m

        // Multi-scattering (dimensionless)
        float MultiScatterContribution = 0.1f;
        float MultiScatterOcclusion = 0.5f;
        float MultiScatterEccentricity = 0.5f;
        float PhaseGForward = 0.7f;
        float PhaseGBackward = -0.3f;
        float MultiScatterBaseG = 0.8f;

        // Powder effects (dimensionless)
        float PowderStrength = 0.7f;
        float PowderEdgeWidth = 0.1f;

        // Cloud ambient lighting (colors and a dimensionless strength)
        float3 AmbientTop = { 0.6f, 0.7f, 0.9f };
        float3 AmbientBottom = { 0.3f, 0.25f, 0.2f };
        float  AmbientStrength = 0.f;

        // Noise detail (dimensionless)
        float DetailErosionStrength = 0.2f;

        // ---- Cloud‑specific distances (meters) ----
        // Stored already scaled. Modify them directly or via ApplyEarthScale().
        float CloudsLayerMin = kCloudsLayerMinRef;
        float CloudsLayerMax = kCloudsLayerMaxRef;
        float CloudsFadeDistance = kCloudsFadeDistRef;
    };

    struct SkyParams
    {
        // Rayleigh and Mie scattering (wavelengths, turbidity, anisotropy – unscaled)
        float3 RayleighWaveLengths = kRefRayleighWaveLengths;
        float  Turbidity = 1.f;
        float  MieAnisotropy = 0.76f;

        // ---- Sky‑specific distance ----
        // Stored already scaled.
        float AtmosHeight = kAtmosHeightRef;
    };

    // ------------------------------------------------------------
    // Main members (all start with capital letter)
    // ------------------------------------------------------------
    AmbientParams Ambient;
    SunParams     Sun;
    CloudsParams  Clouds;
    SkyParams     Sky;

    // ---- Common planet parameters (shared by sky and clouds) ----
    float EarthRadius = kEarthRadiusRef;   // Effective planet radius (scaled)
    float3 EarthCenter = { 0.f, 0.f, 0.f }; // World‑space center of the planet

    // ---- Wind velocity (base value, unscaled) ----
    // Should be multiplied by EarthScaleFactor at binding time.
    float2 WindVelocity = { 0.002f, 0.001f };

    // ---- Current scale factor ----
    float EarthScaleFactor = 1.0f;

    // ------------------------------------------------------------
    // Apply a global scale factor to all scalable distance parameters.
    // Overwrites EarthRadius, AtmosHeight, CloudsLayerMin/Max, and FadeDistance.
    // Frequencies and densities are left unscaled (they need inverse scaling at bind time).
    // ------------------------------------------------------------
    void ApplyEarthScale(float scale)
    {
        EarthScaleFactor = scale;
        EarthRadius = kEarthRadiusRef * scale;
        Sky.AtmosHeight = kAtmosHeightRef * scale;
        Clouds.CloudsLayerMin = kCloudsLayerMinRef * scale;
        Clouds.CloudsLayerMax = kCloudsLayerMaxRef * scale;
        Clouds.CloudsFadeDistance = kCloudsFadeDistRef * scale;
    }

    // ------------------------------------------------------------
    // Set EarthRadius, optionally keeping relative proportions of the
    // other scalable distance parameters.
    // If keepRelativeScale == true, the distance parameters are scaled
    // by the ratio (newRadius / oldRadius), and EarthScaleFactor is updated.
    // Otherwise, only EarthRadius is changed and EarthScaleFactor becomes 1.
    // ------------------------------------------------------------
    void SetEarthRadius(float r, bool keepRelativeScale)
    {
        if (keepRelativeScale)
        {
            float oldRadius = EarthRadius;
            float ratio = (oldRadius != 0.f) ? (r / oldRadius) : 1.f;

            EarthRadius = r;
            Clouds.CloudsLayerMin *= ratio;
            Clouds.CloudsLayerMax *= ratio;
            Clouds.CloudsFadeDistance *= ratio;
            Sky.AtmosHeight *= ratio;

            EarthScaleFactor = r / kEarthRadiusRef;
        }
        else
        {
            EarthRadius = r;
            EarthScaleFactor = 1.0f;
        }
    }

    void SetEarthCenter(const float3& c) { EarthCenter = c; }
    // Direction of light rays of the sun (from sun to camera)
    float3 GetSunDirection() const;

    rhi::TextureHandle GetCloudsShapeTexture() const;
    rhi::TextureHandle GetCloudsDetailTexture() const;

    void InitCloudsTextures(bool forceNew, bool cache, alm::gfx::DeviceManager* deviceManager);

private:

    rhi::TextureOwner m_CloudsShapeTexture;
    rhi::TextureOwner m_CloudsDetailTexture;
};

} // namespace alm::gfx