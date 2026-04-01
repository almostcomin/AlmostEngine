#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on NVidia's Donut sample: https://github.com/NVIDIA-RTX/Donut

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

// Returns a random valuer in the range [-1, 1]
float2 random2(float2 st)
{
    st = float2(dot(st, float2(127.1, 311.7)), dot(st, float2(269.5, 183.3)));
    return -1.0 + 2.0 * frac(sin(st) * 43758.5453123);
}

// Gradient Noise by Inigo Quilez - iq/2013
// https://www.shadertoy.com/view/XdXGW8
// Returns a noise value in the range [-1, 1]
float noise(float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);

    // Gradients for each corner
    float2 g00 = random2(i + float2(0.0, 0.0));
    float2 g10 = random2(i + float2(1.0, 0.0));
    float2 g01 = random2(i + float2(0.0, 1.0));
    float2 g11 = random2(i + float2(1.0, 1.0));
    
    // Offset vectors
    float2 d00 = f - float2(0.0, 0.0);
    float2 d10 = f - float2(1.0, 0.0);
    float2 d01 = f - float2(0.0, 1.0);
    float2 d11 = f - float2(1.0, 1.0);
    
    // Dot products gradient.offset
    float n00 = dot(g00, d00);
    float n10 = dot(g10, d10);
    float n01 = dot(g01, d01);
    float n11 = dot(g11, d11);
    
    float2 u = smoothstep(0, 1, f);
    
    return lerp(lerp(n00, n10, u.x), 
                lerp(n01, n11, u.x), u.y);
}

float3 ProceduralSky(interop::SkyData params, float3 direction, float angularSizeOfPixel)
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
    ConstantBuffer<interop::SkyData> skyData = ResourceDescriptorHeap[Constants.skyDataDI];
    
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

    //---
    
    float2 x = input.uv * 10;
    x.x *= skyData.aspect;
    
    //float r = rand(input.uv);
    //color = float3(r, r, r);
    
/*        
    float2 i = floor(x);
    float2 f = frac(x); 

    float a = rand(i);
    float b = rand(i + float2(1, 0));
    float c = rand(i + float2(0, 1));
    float d = rand(i + float2(1, 1));
    
    float2 u = smoothstep(0, 1, f);
    
    float y = lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
    
    color = float3(y, y, y);
  */
    
    float y = noise(x) * 0.5 + 0.5;
    color = float3(y, y, y);
            
    return float4(color, 1.0);
}