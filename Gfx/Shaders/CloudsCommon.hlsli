#ifndef __CLOUDS_COMMON_HLSLI__
#define __CLOUDS_COMMON_HLSLI__

#include "Common.hlsli"

// Stratus: thin band near base.
float StratusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.10, norY);
    float rampOut = 1.0 - smoothstep(0.20, 0.25, norY);
    return saturate(rampIn * rampOut);
}

// Cumulus: base lifts from 0.05 to 0.20, wispy top fade 0.55-0.85.
float CumulusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.20, norY);
    float rampOut = 1.0 - smoothstep(0.55, 0.70, norY);
    return saturate(rampIn * rampOut);
}

// Cumulonimbus: sharper base lift 0.05-0.15, top fade only at 0.90-1.00.
float CumulonimbusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.15, norY);
    float rampOut = 1.0 - smoothstep(0.90, 1.00, norY);
    return saturate(rampIn * rampOut);
}

// Weighted blend of the three profiles
float CloudCoverageShape(float norY, float stratusWeight, float cumulusWeight, float cumulonimbusWeight)
{
    float s = StratusProfile(norY) * stratusWeight
            + CumulusProfile(norY) * cumulusWeight
            + CumulonimbusProfile(norY) * cumulonimbusWeight;
    return saturate(s);
}

// Density 1 at the base, 0.3 at the top.
float GlobalHeightGradient(float norY)
{
    return lerp(1.0, 0.55, smoothstep(0.0, 1.0, norY));
}

float SampleCloudDensity(float3 pos, float norY, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer<interop::CloudsShapeData> cloudsShape)
{    
    // Si norY está fuera del rango válido [0,1], no hay densidad
    if (norY < 0.0 || norY > 1.0)
        return 0.0;

    // --- BASE SHAPE

    float3 uvw1;
    uvw1.xy = pos.xz * cloudsShape.ShapeScale;
    uvw1.xy += cloudsShape.WindOffset;
    uvw1.z = norY;
    float3 uvw2;
    uvw2.xy = pos.xz * cloudsShape.ShapeScale * 0.37; // prime number
    uvw2.xy += cloudsShape.WindOffset * 0.73;
    uvw2.z = frac(norY + 0.5);
        
    float4 noise1 = cloudsTexture.SampleLevel(linearWrapSampler, uvw1, 0.0);
    float4 noise2 = cloudsTexture.SampleLevel(linearWrapSampler, uvw2, 0.0);
    float4 noise = lerp(noise1, noise2, 0.3);

    float perlinWorley = noise.r;
    float3 worley = noise.gba;
    
    // cloud shape modeled after the GPU Pro 7 chapter
    float wfbm = worley.r * 0.625 + worley.g * 0.25 + worley.b * 0.125;
    float coverage = remap(perlinWorley, wfbm - 1.0, 1.0, 0.0, 1.0);
    coverage = remap(coverage, 1.0 - cloudsShape.Coverage, 1.0, 0.0, 1.0);

    if (coverage <= 0.0)
        return 0.0;

    coverage *= CloudCoverageShape(norY, cloudsShape.StratusWeight, cloudsShape.CumulusWeight, cloudsShape.CumulonimbusWeight);
    coverage *= GlobalHeightGradient(norY);
    coverage = saturate(coverage);

    // -- DETAIL EROSION

    float3 detailUVW;
    detailUVW.xy = pos.xz * cloudsShape.ShapeScale * cloudsShape.DetailScale;
    detailUVW.xy += cloudsShape.WindOffset * cloudsShape.DetailScale;
    detailUVW.z  = norY;

    float3 detail = cloudsDetailTexture.SampleLevel(linearWrapSampler, detailUVW, 0.0).rgb;
    float hfbm = detail.r * 0.625 + detail.g * 0.25 + detail.b * 0.125;

    // Per paper: in the lower half (near the layer floor),
    // Worley is inverted -> produces wispy/transparent edges.
    // norY E [0,1]: lower values -> wispy, higher values -> billowy (cumulus-like).
    hfbm = lerp(1.0 - hfbm, hfbm, smoothstep(0.0, 0.5, norY));

    // Dynamic threshold driven by detail strength.
    // Any density value below `hfbm*strength` is clamped to zero.
    float threshold = hfbm * cloudsShape.DetailErosionStrength;
    float eroded = remap(coverage, threshold, 1.0, 0.0, 1.0);

    return saturate(eroded);
}

bool GetCloudsLayerIntersectionPoints(
    float3 rayOrigin,
    float3 rayDir,
    float earthRadius,       // Solid Earth radius
    float innerSphereRadius, // Cloud base altitude (earthRadius + cloudMinHeight)
    float outerSphereRadius, // Cloud top altitude (earthRadius + cloudMaxHeight)
    float tMax,              // Additional solid occluder (e.g., depth buffer for mountains/buildings)
    out float tEntry,
    out float tExit)
{
    tEntry = 0.0;
    tExit = 0.0;

    // Intersections with cloud layer boundaries
    float2 innerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), innerSphereRadius);
    float2 outerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), outerSphereRadius);

    // If the outer sphere is entirely behind the origin or misses it, no clouds ahead
    if (outerHit.y < 0.0) 
        return false;

    // Entry: start at outerHit.x or 0 if inside, or innerHit.y if inside inner sphere
    tEntry = max(max(outerHit.x, 0.0), (innerHit.x < 0.0 && innerHit.y > 0.0) ? innerHit.y : 0.0);

    // Exit: always go to outer boundary, let density be zero inside inner sphere
    tExit = outerHit.y;

    // Solid Earth occlusion
    float2 earthHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), earthRadius);
    if (length(rayOrigin) >= earthRadius)
    {
        if (earthHit.x > 0.0)
            tExit = min(tExit, earthHit.x);
    }
    else
    {
        if (earthHit.y > 0.0)
            tEntry = max(tEntry, earthHit.y);
        else
            return false;
    }

    // Solid geometry occlusion
    tExit = min(tExit, tMax);

    if (tEntry >= tExit)
        return false;

    return true;
}

// Step-decorrelated jitter.
// pixelPos in pixels, step = sample ID. Returns [0, 1].
float StepJitter(float2 pixelPos, uint step, float salt)
{
    // Golden ratio breaks step-to-step correlation.
    // Salt prevents main/shadow jitter periodicity conflicts.
    return frac(InterleavedGradientNoise(pixelPos + float2(step, step * 1.61803398875)) + salt);
}

#endif // __CLOUDS_COMMON_HLSLI__