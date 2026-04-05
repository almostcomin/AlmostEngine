#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Noise.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float CLOUD_RADIUS = 1000.0; // Radius of the skydome    
static const float CLOUD_DENSITY_POW = 2.0f;
static const float SUN_SCATTER_POW = 4.0f;
static const float SUN_CLOUD_POW = 2.0f;
static const float CLOUD_FADE_START = 0.05f;
static const float CLOUD_FADE_RANGE = 0.15f;
static const float CLOUD_FADE_POW = 1.5f;

float GetCloudDensity(float v)
{
    //v = NormalizeNoise(v);
    return pow(v, CLOUD_DENSITY_POW);
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 worldPos = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 worldDir = normalize(worldPos.xyz);
        
    // Planar projection
    float3 cloudPos = worldDir * (CLOUD_RADIUS / max(worldDir.y, 0.0001f));
    float2 st = cloudPos.xz * Constants.cloudScale;
    st += Constants.windVelocity * Constants.time;
        
    // Sample noise and convert to density via GetCloudDensity
    //float noiseValue = GradientFbm(st, 6);
    float noiseValue = Turbulence(st, 6, 0.5);
    float cloudDensity = GetCloudDensity(noiseValue);

    // Horizon mask
    float horizonMask = saturate((worldDir.y - CLOUD_FADE_START) / CLOUD_FADE_RANGE);
    cloudDensity *= pow(horizonMask, CLOUD_FADE_POW);
    
    // Sun direction
    float3 sunDir = normalize(Constants.sunDirection);
    float sunDot = max(0.0f, dot(worldDir, sunDir));
    float sunScatter = pow(sunDot, SUN_SCATTER_POW);

    // Sky gradient: zenith-to-horizon + directional sun scatter
    float3 skyZenith = float3(0.00f, 0.10f, 0.20f);
    float3 skyHorizon = float3(0.10f, 0.20f, 0.40f);
    float3 sunColor = float3(1.00f, 0.75f, 0.45f); // warm scatter near sun

    float3 skyGradient = lerp(skyHorizon, skyZenith, saturate(worldDir.y));
    skyGradient = lerp(skyGradient, sunColor, sunScatter * 0.35f);

    // Cloud color: shadowed vs. lit by sun
    float3 cloudShadow = float3(0.70f, 0.70f, 0.80f);
    float3 cloudLit = float3(1.00f, 1.00f, 0.90f);
    float3 cloudColor = lerp(cloudShadow, cloudLit, pow(sunDot, SUN_CLOUD_POW));

    float3 color = lerp(skyGradient, cloudColor, cloudDensity);
    return float4(color, 1.0f);
}