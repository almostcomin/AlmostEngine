#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Noise.hlsli"
#include "Common.hlsli"

#define EARTH_RADIUS    (1500000.) // (6371000.)

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct CloudResult
{
    float3 color;
    float transmittance;
    float weightedDistance; // density-weighted average distance
};

float InterectCloudSphere(float3 rd, float r)
{
    float b = EARTH_RADIUS * rd.y;
    float d = b * b + r * r + 2.0 * EARTH_RADIUS * r;
    return -b + sqrt(d);
}

float SampleCloudDensity(float3 pos, Texture3D cloudsTexture, ConstantBuffer<interop::CloudsData> cloudsData)
{
    float cloudsLayerHeight = saturate((pos.y - cloudsData.cloudLayerMin) / (cloudsData.cloudLayerMax - cloudsData.cloudLayerMin));
    
    float3 uvw1;
    uvw1.xy = pos.xz * cloudsData.cloudScale;
    uvw1.xy += cloudsData.windVelocity * Constants.time;
    uvw1.z = cloudsLayerHeight;
    float3 uvw2;
    uvw2.xy = pos.xz * cloudsData.cloudScale * 0.37f; // prime number
    uvw2.xy += cloudsData.windVelocity * Constants.time;
    uvw2.z = frac(cloudsLayerHeight + 0.5);
        
    float4 noise1 = cloudsTexture.SampleLevel(linearWrapSampler, uvw1, 0.0);
    float4 noise2 = cloudsTexture.SampleLevel(linearWrapSampler, uvw2, 0.0);
    float4 noise = lerp(noise1, noise2, 0.4);

    float perlinWorley = noise.r;
    float3 worley = noise.gba;
    
    // cloud shape modeled after the GPU Pro 7 chapter
    float wfbm = worley.r * 0.625 + worley.g * 0.25 + worley.b * 0.125;
    float coverage = remap(perlinWorley, wfbm - 1.0, 1.0, 0.0, 1.0);
    coverage = remap(coverage, 1.0 - cloudsData.coverage, 1.0, 0.0, 1.0);
    coverage = saturate(coverage);
    //coverage = pow(coverage, 2.0f);

    // Gradiente de densidad vertical — nubes más densas en el centro de la capa
    //float heightGradient = saturate(remap(uvw.z, 0.0f, 0.1f, 0.0f, 1.0f))   // fade inferior
    //                     * saturate(remap(uvw.z, 0.6f, 1.0f, 1.0f, 0.0f));  // fade superior
    //coverage *= heightGradient;
    
    return coverage;
}

float SampleLightRay(float3 pos, Texture3D cloudsTexture, ConstantBuffer<interop::CloudsData> cloudsData)
{
    float tSunExit = RayPlaneIntersection(pos, cloudsData.toSunDirection, cloudsData.cloudLayerMax);
    if (tSunExit <= 0.0)
    {
        //return 0.0; // sol under the horizon
        return  0.15;
    }
    
    float stepSize = tSunExit / cloudsData.lightSteps;
    
    float density = 0.0;
    for (int i = 0; i < cloudsData.lightSteps; i++)
    {
        float3 lightPos = pos + cloudsData.toSunDirection * stepSize * (i + 0.5);
        density += SampleCloudDensity(lightPos, cloudsTexture, cloudsData);
    }
    density /= cloudsData.lightSteps;

    // Beer-Lambert towards sun
    return exp(-density * stepSize * cloudsData.absorptionCoeff);
}

CloudResult GetCloudsColorRayMarch(float3 rayOrigin, float3 rayDir, Texture3D cloudsTexture, float jitter, ConstantBuffer<interop::CloudsData> cloudsData)
{
    CloudResult result;
    result.color = 0.0;
    result.transmittance = 1.0;
    result.weightedDistance = 0.0;
    
    float tEntry = 0.0;
    if (rayOrigin.y < cloudsData.cloudLayerMin)
        tEntry = RayPlaneIntersection(rayOrigin, rayDir, cloudsData.cloudLayerMin);
    else if (rayOrigin.y > cloudsData.cloudLayerMax)
        tEntry = RayPlaneIntersection(rayOrigin, rayDir, cloudsData.cloudLayerMax);
    if (tEntry < 0.0)
        return result;
    
    float tExit = 0.0;
    if (rayDir.y > 0.0)
        tExit = RayPlaneIntersection(rayOrigin, rayDir, cloudsData.cloudLayerMax);
    else
        tExit = RayPlaneIntersection(rayOrigin, rayDir, cloudsData.cloudLayerMin);
    if (tExit < tEntry)
        return result;
    
    float stepSize = (tExit - tEntry) / cloudsData.maxSteps;
    float weightPerStep = cloudsData.absorptionCoeff * stepSize;
    float t = tEntry + jitter * stepSize;        
    float totalDensity = 0.0;
    
    for (uint i = 0; i < cloudsData.maxSteps; ++i)
    {
        float3 pos = rayOrigin + rayDir * t;
        float density = SampleCloudDensity(pos, cloudsTexture, cloudsData);
        if (density > 0.0)
        {            
            float absorption = exp(-density * stepSize * cloudsData.absorptionCoeff);
            float lightEnergy = SampleLightRay(pos, cloudsTexture, cloudsData);
            lightEnergy += 0.2; // ambient

            float3 S = lightEnergy * density;
            float dTrans = absorption;
            float3 Sint = (S - S * dTrans) * (1.0f / max(density, 0.0001f)); // analytical integral
            
            result.color += result.transmittance * Sint;
            result.transmittance *= dTrans;
            result.weightedDistance += t * density;

            totalDensity += density;
            if (result.transmittance < 0.001)
                break;
        }
        
        t += stepSize;
    }    
    result.weightedDistance /= max(totalDensity, 0.0001f);

    return result;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::CloudsData> cloudsData = ResourceDescriptorHeap[Constants.cloudsDataDI];    
    Texture3D baseTexture = ResourceDescriptorHeap[cloudsData.cloudBaseShapeTexture];
    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 worldPos = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 worldDir = normalize(worldPos.xyz);
        
    CloudResult clouds = GetCloudsColorRayMarch(
        float3(0.0f, 0.0f, 0.0f), worldDir, baseTexture, hash(input.uv /*+ frac(Constants.time)*/), cloudsData);
    
    // Horizon mask
    //float horizonMask = saturate((worldDir.y - CLOUD_FADE_START) / CLOUD_FADE_RANGE);
    //cloudsColor *= pow(horizonMask, CLOUD_FADE_POW);
    
    // Sun direction
    //float3 sunDir = normalize(cloudsData.toSunDirection);
    //float sunDot = max(0.0f, dot(worldDir, sunDir));
    //float sunScatter = pow(sunDot, SUN_SCATTER_POW);

    // Sky gradient: zenith-to-horizon + directional sun scatter
    float3 skyZenith = float3(0.08f, 0.39f, 0.84f);
    float3 skyHorizon = float3(0.50f, 0.61f, 0.80f);
    float3 sunColor = float3(1.00f, 0.75f, 0.45f); // warm scatter near sun

    float3 skyGradient = lerp(skyHorizon, skyZenith, pow(saturate(worldDir.y), 0.5));
    //skyGradient = lerp(skyGradient, sunColor, sunScatter * 0.35f);

    float distFade = 1.0 - saturate(clouds.weightedDistance / cloudsData.cloudFadeDistance);
    float3 cloudColor = clouds.color * float3(1.0f, 0.95f, 0.9f);
    float3 color = skyGradient * clouds.transmittance + cloudColor;
    color = lerp(skyGradient, color, distFade);
            
    return float4(color, 1.0f);
}