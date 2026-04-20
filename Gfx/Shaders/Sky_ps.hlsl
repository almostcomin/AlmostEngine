#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on Fast skycolor function by Íñigo Quílez
// https://www.shadertoy.com/view/MdX3Rr

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 GetSkyColor(float3 rayDir, float3 sunDir)
{
    float sunDot = saturate(dot(rayDir, sunDir));
    
    // Base gradient (zenith to horizon)
    float3 col = float3(0.2, 0.5, 0.85) * 1.1 - max(rayDir.y, 0.01) * max(rayDir.y, 0.01) * 0.5;
    
    // Horizon haze
    col = lerp(col, 0.85 * float3(0.7, 0.75, 0.85), pow(1.0 - max(rayDir.y, 0.0), 6.0));
    
    // Sun halo (multiple scales for glow)
    col += 0.25 * float3(1.0, 0.7, 0.4) * pow(sunDot, 5.0);
    col += 0.25 * float3(1.0, 0.8, 0.6) * pow(sunDot, 64.0);
    col += 0.20 * float3(1.0, 0.8, 0.6) * pow(sunDot, 512.0);
    
    // Below horizon darkening
    col += saturate((0.1 - rayDir.y) * 10.0) * float3(0.0, 0.1, 0.2);
    
    // Extra soft glow near sun
    col += 0.2 * float3(1.0, 0.8, 0.6) * pow(sunDot, 8.0);

    return col;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos); // homogeneous ray direction
    float3 rayDir = normalize(rayDirH.xyz);
    
    float3 color = GetSkyColor(rayDir, Constants.toSunDirection);         
    return float4(color, 1.0);
}