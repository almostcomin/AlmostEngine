#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on Fast skycolor function by Íñigo Quílez
// https://www.shadertoy.com/view/MdX3Rr

#define TINT_TIGHT  float3(1.0, 1.0, 1.0) // near disk -- no tint
#define TINT_MEDIUM float3(1.0, 0.95, 0.85) // little 
#define TINT_WIDE   float3(1.0, 0.85, 0.65) // more tint

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 GetSkyColor(float3 rayDir, ConstantBuffer<interop::SkyData> skyData)
{    
    float sunDot = saturate(dot(rayDir, skyData.ToSunDirection));
    float y = max(rayDir.y, 0.01);
    
    // Base gradient (zenith to horizon)
    float3 col = skyData.ZenitColor - (y*y) * skyData.ZenitFalloff;
    
    // Horizon haze
    col = lerp(col, skyData.HorizonColor, pow(1.0 - max(rayDir.y, 0.0), skyData.HorizonFalloff));
    
    // Sun halo (multiple scales for glow)
    col += 0.25 * skyData.SunColor * TINT_WIDE * pow(sunDot, 5.0);
    col += 0.25 * skyData.SunColor * TINT_MEDIUM * pow(sunDot, 64.0);
    col += 0.20 * skyData.SunColor * TINT_TIGHT * pow(sunDot, 512.0);
    
    // Below horizon darkening
    col += saturate((0.1 - rayDir.y) * skyData.GroundFalloff) * skyData.GroundColor;
        
    // Extra soft glow near sun
    col += 0.2 * skyData.SunColor * TINT_TIGHT * pow(sunDot, 8.0);

    return col;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::SkyData> skyData = ResourceDescriptorHeap[Constants.SkyDataDI];
    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos); // homogeneous ray direction
    float3 rayDir = normalize(rayDirH.xyz);
    
    float3 color = GetSkyColor(rayDir, skyData);
    return float4(color, 1.0);
}