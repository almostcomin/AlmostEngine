#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

// Based on the wonderful ScrathPixel post
// https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
// And in the sky shader from StillTravelling
// https://www.shadertoy.com/view/tdSXzD

#define DENSITY 0.5
#define ZENIT_OFFSET 0.48

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
#if 0
float GetZenitDensity(float x)
{
    return DENSITY / pow(max(x - ZENIT_OFFSET, 0.0035), 0.75);
}

float3 GetAtmosphericScattering(float3 rayDir, ConstantBuffer<interop:: SkyData> skyData)
{
    float zenitDensity = GetZenitDensity(rayDir.y);
    
}
#endif

float3 Scatter(float3 rayOriginLocal, float3 rayDir, ConstantBuffer <interop::SkyData> skyData)
{
    // Get actual ray origin
    float3 rayOrigin = rayOriginLocal - skyData.EarthCenter;

    float2 atmosHit = RaySphereIntersection(rayOrigin, rayDir, skyData.AtmosRadius);
    float2 earthHit = RaySphereIntersection(rayOrigin, rayDir, skyData.EarthRadius);

    if(atmosHit.y < 0.0)
        return float3(0.0, 0.0, 0.0); // No atmosphere, looking at space

    // Determine raymarch range
    float tStart = max(atmosHit.x, 0.0);
    float tEnd = atmosHit.y;
    if(earthHit.x > 0.0)
        tEnd  = min(tEnd, earthHit.x);

    float L = tEnd - tStart;
    float dL = L / skyData.NumSteps;

    float3 accumR = 0.0;
    float3 accumM = 0.0;
    float depthR = 0.0;
    float depthM = 0.0;

    // Raymarch from rayOrigin to the exit of the atmosphere
    for (uint i = 0; i < skyData.NumSteps; ++i)
    {
        float t = tStart + dL * (i + 0.5); // sample at center of segment
        float3 p = rayOrigin + rayDir * t;
        float h = length(p) - skyData.EarthRadius; // Height of p from surface
        
        float dR = exp(-h / skyData.Hr) * dL;
        float dM = exp(-h / skyData.Hm) * dL;
        
        depthR += dR;
        depthM += dM;

        // Inner raymarch, from p towards sun
        float2 lightAtmosHit = RaySphereIntersection(p, skyData.ToSunDirection, skyData.AtmosRadius);
        float2 lightEarthHit = RaySphereIntersection(p, skyData.ToSunDirection, skyData.EarthRadius);

        // If sun ray hits the earth from point p -- this point is in shadow
        if (lightEarthHit.x > 0.0)
        {
            // Skip this point completely -- no sun light reaches it
            continue;
        }

        float dLs = lightAtmosHit.y / skyData.NumLightSteps;
        float depthRs = 0.0;
        float depthMs = 0.0;
#if 1
        for(uint j = 0; j < skyData.NumLightSteps; ++j)
        {
            float ts = dLs * (j + 0.5);
            float3 ps = p + skyData.ToSunDirection * ts;
            float hs = length(ps) - skyData.EarthRadius; // Height of p from surface

            depthRs += exp(-hs / skyData.Hr) * dLs;
            depthMs += exp(-hs / skyData.Hm) * dLs;
        }
#endif
        // Total transmittance from sun to camera passing through p
        float3 T = exp(-(skyData.bR * (depthR + depthRs) + skyData.bM * (depthM + depthMs)));

        accumR += T * dR;
        accumM += T * dM;
    }

    // Phases
    float mu = dot(rayDir, skyData.ToSunDirection);
    float mu2 = square(mu);

    float phaseR = (3.0 / (16.0 * M_PI)) * (1.0 + mu2);

    float g2 = square(skyData.G);
    float phaseM = (3.0 / (8.0 * M_PI)) * 
                   ((1.0 - g2) * (1.0 + mu2)) /
                   ((2.0 + g2) * pow(1.0 + g2 - 2.0 * skyData.G * mu, 1.5));

    float3 color = skyData.SunIntensity * (skyData.bR * phaseR * accumR + skyData.bM * phaseM * accumM);

    //color = float3(L / 100000.0, L / 100000.0, L / 100000.0);
    return color;    
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
    
    float3 color = Scatter(Constants.CameraPosition, rayDir, skyData);
    return float4(color, 1.0);
}