#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on NVidia's Donut sample: https://github.com/NVIDIA-RTX/Donut

ConstantBuffer<interop::SimpleSkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 ProceduralSky(interop::SimpleSkyData params, float3 direction, float angularSizeOfPixel)
{
    float elevation = asin(clamp(dot(direction, params.directionUp), -1.0, 1.0));    
    float top = smoothstep(0.f, params.horizonSize, elevation);
    float bottom = smoothstep(0.f, params.horizonSize, -elevation);
    float3 environment = lerp(lerp(params.horizonColor, params.groundColor, bottom), params.skyColor, top);

    float angleToLight = acos(saturate(dot(direction, params.directionToLight)));
    float halfAngularSize = params.angularSizeOfLight * 0.5;
    float lightIntensity = saturate(1.0 - smoothstep(halfAngularSize - angularSizeOfPixel * 2, halfAngularSize + angularSizeOfPixel * 2, angleToLight));
    lightIntensity = pow(lightIntensity, 4.0);
    float glowInput = saturate(2.0 * (1.0 - smoothstep(halfAngularSize - params.glowSize, halfAngularSize + params.glowSize, angleToLight)));
    float glowIntensity = params.glowIntensity * pow(glowInput, params.glowSharpness);
    float3 light = max(lightIntensity, glowIntensity) * params.lightColor;
    
    return environment + light;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::SimpleSkyData> skyData = ResourceDescriptorHeap[Constants.skyDataDI];
    
    float4 clipPos;
    clipPos.x = input.uv.x * 2 - 1;
    clipPos.y = 1 - input.uv.y * 2;
    clipPos.z = 0.5;
    clipPos.w = 1;
    float4 translatedWorldPos = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 direction = normalize(translatedWorldPos.xyz / translatedWorldPos.w);
    
    // length(ddx(dir)) is an approximation for acos(dot(dir, normalize(dir + ddx(dir))) for unit vectors with small derivatives
    float angularSizeOfPixel = max(length(ddx(direction)), length(ddy(direction)));
    float3 color = ProceduralSky(skyData, direction, angularSizeOfPixel);
         
    return float4(color, 1.0);
}