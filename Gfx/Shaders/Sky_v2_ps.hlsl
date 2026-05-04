#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

// Based on the ScrathPixel post
// https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
// And in the sky shader from StillTravelling
// https://www.shadertoy.com/view/tdSXzD
// And Sebastian Lague video
// https://www.youtube.com/watch?v=DxfEbulyFcY

#define DENSITY 0.5
#define ZENIT_OFFSET 0.48

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};



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

        for(uint j = 0; j < skyData.NumLightSteps; ++j)
        {
            float ts = dLs * (j + 0.5);
            float3 ps = p + skyData.ToSunDirection * ts;
            float hs = length(ps) - skyData.EarthRadius; // Height of p from surface

            depthRs += exp(-hs / skyData.Hr) * dLs;
            depthMs += exp(-hs / skyData.Hm) * dLs;
        }

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

    // Sun disk
    if (earthHit.x < 0.0) // no earth hit, sun visible
    {
        // Sun disk -- apparent size depends on elevation
        float sunElevation = skyData.ToSunDirection.y; // 1 = zenith, 0 = horizon, <0 = below

        // Optical illusion factor: sun appears larger near horizon
        // At zenith: factor = 1 (real size)
        // At horizon: factor up to 10
        float horizonBoost = 1.0 + 9.0 * pow(saturate(1.0 - sunElevation), 8.0);

        float effectiveAngularRadius = skyData.SunAngularRadius * horizonBoost;
        float effectiveAngularRadiusCos = cos(effectiveAngularRadius);

        if(mu > effectiveAngularRadiusCos)
        {
            // Angle between view direction and sun center
            float angle = acos(mu);
            // Radial position on the sun disk (0 = center, 1 = edge)
            float diskPos = saturate(angle / effectiveAngularRadius);
            // Mu for the disk point (cosine of the exit angle from the sun's surface)
            float muSun = sqrt(1.0 - square(diskPos));
            // Quadratic limb darkening model
            float limbDarkening = 1.0 - 0.6 * (1.0 - muSun) - 0.4 * (1.0 - muSun) * (1.0 - muSun);
            // Anti-aliased edge -- fades to 0 over the last 'falloff' radians
            float edgeAA = saturate((1.0 - diskPos) / skyData.SunEdgeAAFalloff);
            // Atmospheric transmittance from camera to edge of atmosphere along this ray
            float3 transmittance = exp(-(skyData.bR * depthR + skyData.bM * depthM));

            color += skyData.SunRadiance * transmittance * limbDarkening * edgeAA;
        }
    }

     return color;
}

float DensityAtPoint(float3 densitySamplePoint, float3 earthCenter, float earthRadius, float atmosRadius)
{
    float altitude = lenght(densitySamplePoint - earthCenter) - earthRadius;
    float altitude01 = altitude / (atmosRadius - earthRadius);
    float localDensity = exp(-altitude01);
    return localDensity;
}

float AvgAtmosDensity(float3 rayOrigin, float3 rayDir, float rayLength, ConstantBuffer<interop::SkyData> skyData)
{
    float3 samplePoint = rayOrigin;
    float stepSize = rayLength / (skyData.NumLightSteps - 1);
    float avgDensity = 0;
    
    for(int i = 0; i < skyData.NumLightSteps; ++i)
    {
        float localDensity = DensityAtPoint(samplePoint);
        avgDensity += localDensity * stepSize;
        samplePoint += rayDir * stepSize;
    }
    return avgDensity;
}

float CalculateLight(float3 rayOrigin, float3 rayDir, float rayLength, ConstantBuffer<interop::SkyData> skyData)
{
    float3 scatterPoint = rayOrigin;
    float stepSize = rayLength / skyData.NumSteps;
    float scatteredLight = 0;
    for(int i = 0; i < skyData.NumSteps; ++i)
    {
        float2 sunRayHit = RaySphereIntersection(scatterPoint, skyData.ToSunDirection, skyData.EarthCenter, skyData.AtmosRadius);
        float sunRayAvgDensity = AvgAtmosDensity(scatterPoint, skyData.ToSunDirection, sunRayHit.y);
        float viewRayAvgDensity = AvgAtmosDensity(scatterPoint, -rayDir, stepSize * i);

        float transmittance = exp(-sunRayAvgDensity - viewRayAvgDensity);
        float localDensity = DensityAtPoint(scatterPoint, skyData.EarthCenter, skyData.EarthRadius, skyData.AtmosRadius);
        
        scatteredLight += localDensity * transmittance * stepSize;
        scatterPoint += rayDir * stepSize;        
    }

    return scatteredLight;
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
    float3 rayOrigin = Constants.CameraPosition;
    
    float2 atmosHit = RaySphereIntersection(rayOrigin, rayDir, skyData.EarthCenter, skyData.AtmosRadius);
    if (atmosHit.y < 0.0)
    {
        return float3(0.0, 0.0, 0.0); // No atmosphere, looking at space    
    }
    
    float tStart = max(atmosHit.x, 0.0);
    float tEnd = atmosHit.y;
    
    
    
    

    
    
    float3 color = Scatter(Constants.CameraPosition, rayDir, skyData);
    return float4(color, 1.0);
}