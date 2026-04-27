#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

// Based on the wonderful ScrathPixel post
// https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
// And in the sky shader from StillTravelling
// https://www.shadertoy.com/view/tdSXzD

#define EARTH_RADIUS 6360000
#define ATMOS_RADIUS 6420000

#define DENSITY 0.5
#define ZENIT_OFFSET 0.48
#define NUM_STEPS 128
#define NUM_LIGHT_STEPS 32

#define Hr 8000.0  // Rayleigh scale height: 8 km
#define Hm 1200.0  // Mie scale height: 1.2 km

// Scattering coefficients at sea level (per meter)
#define bR float3(5.8e-6, 13.5e-6, 33.1e-6) // Rayleigh
#define bM float3(21e-6, 21e-6, 21e-6)      // Mie

// Phase function constants
#define G 0.76 // Mie anisotropy

// Sun intensity
#define SUN_INTENSITY 22.0 // empirical

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
float IntersectAtmosSphere(float3 rd, float r, float earthRadius)
{
    float b = earthRadius * rd.y;
    float d = b * b + r * r + 2.0 * earthRadius * r;
    return -b + sqrt(d);
}

float3 Scatter(float3 rayOriginLocal, float3 rayDir, ConstantBuffer <interop::SkyData> skyData)
{
    // Get actual ray origin
    float3 rayOrigin = rayOriginLocal - skyData.EarthCenter;

    float2 atmosHit = RaySphereIntersection(rayOrigin, rayDir, ATMOS_RADIUS);
    float2 earthHit = RaySphereIntersection(rayOrigin, rayDir, EARTH_RADIUS);

    if(atmosHit.y < 0.0)
        return float3(0.0, 0.0, 0.0); // No atmosphere, looking at space

    // Determine raymarch range
    float tStart = max(atmosHit.x, 0.0);
    float tEnd = atmosHit.y;
    if(earthHit.x > 0.0)
        tEnd  = min(tEnd, earthHit.x);

    float L = tEnd - tStart;
    float dL = L / NUM_STEPS;

    float3 accumR = 0.0;
    float3 accumM = 0.0;
    float depthR = 0.0;
    float depthM = 0.0;

    // Raymarch from rayOrigin to the exit of the atmosphere
    for (uint i = 0; i < NUM_STEPS; ++i)
    {
        float t = tStart + dL * (i + 0.5); // sample at center of segment
        float3 p = rayOrigin + rayDir * (dL * i);
        float h = length(p) - EARTH_RADIUS; // Height of p from surface
        
        float dR = exp(-h / Hr) * dL;
        float dM = exp(-h / Hm) * dL;
        
        depthR += dR;
        depthM += dM;

        // Inner raymarch, from p towards sun
        float2 lightAtmosHit = RaySphereIntersection(p, skyData.ToSunDirection, ATMOS_RADIUS);
        float2 lightEarthHit = RaySphereIntersection(p, skyData.ToSunDirection, EARTH_RADIUS);

        float tLightEnd = lightAtmosHit.y;
        // If sun ray hits the earth from point p -- this point is in shadow
        if (lightEarthHit.x > 0.0)
        {
            // Skip this point completely -- no sun light reaches it
            continue;
        }

        float2 Ls = RaySphereIntersection(p, skyData.ToSunDirection, ATMOS_RADIUS);
        //float dLs = Ls / NUM_LIGHT_STEPS;

        float depthRs = 0.0;
        float depthMs = 0.0;
#if 0
        for(uint j = 0; j < NUM_LIGHT_STEPS; ++j)
        {            
            float3 ps = p + skyData.ToSunDirection * (dLs * j);
            float hs = lenght(ps) - EARTH_RADIUS; // Height of p from surface

            depthRs += exp(-hs / Hr) * dLs;
            depthMs += exp(-hs / Hm) * dLs;
        }
#endif
        // Total transmittance from sun to camera passing through p
        float3 T = exp(-(bR * (depthR + depthRs) + bM * (depthM + depthMs)));

        accumR += T * dR;
        accumM += T * dM;
    }

    // Phases
    float mu = dot(rayDir, skyData.ToSunDirection);
    float mu2 = square(mu);

    float phaseR = (3.0 / (16.0 * M_PI)) * (1.0 + mu2);

    float g2 = square(G);
    float phaseM = (3.0 / (8.0 * M_PI)) * 
                   ((1.0 - g2) * (1.0 + mu2)) /
                   ((2.0 + g2) * pow(1.0 + g2 - 2.0 * G * mu, 1.5));

    float3 color = SUN_INTENSITY * (bR * phaseR * accumR + bM * phaseM * accumM);

    color = float3(L / 100000.0, L / 100000.0, L / 100000.0);
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