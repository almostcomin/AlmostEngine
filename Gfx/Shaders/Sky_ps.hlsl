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

struct CloudResult
{
    float3 color;
    float transmittance;
    float nearDistance; // Distance to the first point with density > 0
    float weightedDistance; // Density-weighted average distance
};

float IntersectCloudSphere(float3 rd, float r, float earthRadius)
{
    float b = earthRadius * rd.y;
    float d = b * b + r * r + 2.0 * earthRadius * r;
    return -b + sqrt(d);
}

float SampleCloudDensity(float3 pos, float norY, Texture3D cloudsTexture, ConstantBuffer<interop::CloudsData> cloudsData)
{    
    float3 uvw1;
    uvw1.xy = pos.xz * cloudsData.cloudsScale;
    uvw1.xy += cloudsData.windOffset;
    uvw1.z = norY;
    float3 uvw2;
    uvw2.xy = pos.xz * cloudsData.cloudsScale * 0.37; // prime number
    uvw2.xy += cloudsData.windOffset * 0.73;
    uvw2.z = frac(norY + 0.5);
        
    float4 noise1 = cloudsTexture.SampleLevel(linearWrapSampler, uvw1, 0.0);
    float4 noise2 = noise1;//cloudsTexture.SampleLevel(linearWrapSampler, uvw2, 0.0);
    float4 noise = lerp(noise1, noise2, 0.3);

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

float VolumetricShadow(float3 from, Texture3D cloudsTexture, ConstantBuffer<interop:: CloudsData> cloudsData)
{
    float dd = 10.0; // Initial step size in meters
    float d = dd * 0.5;
    float shadow = 1.0;

    for(int i = 0; i < cloudsData.lightSteps; ++i)
    {
        float3 pos = from + cloudsData.toSunDirection * d;
        float altitude = length(pos) - cloudsData.earthRadius;
        float norY = (altitude - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness;
        if(norY > 1.0)
            return shadow;

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsData);
        shadow *= exp(-density * dd * cloudsData.absorptionCoeff);
        
        dd *= 1.3f;
        d += dd;
    }
    return shadow;
}

CloudResult GetCloudsColorRayMarch(float3 rayOriginLocal, float3 rayDir, Texture3D cloudsTexture, ConstantBuffer<interop::CloudsData> cloudsData,
    float sceneDist, float2 uv)
{
    CloudResult result;
    result.color = 0.0;
    result.transmittance = 1.0;
    result.nearDistance = cloudsData.earthRadius * 2.0f;
    result.weightedDistance = 0.0;
    
    // Remap ray origin
    float3 rayOrigin = rayOriginLocal - cloudsData.earthCenter;
    
    float tEntry = IntersectCloudSphere(rayDir, cloudsData.cloudLayerMin, cloudsData.earthRadius);
    float tExit = IntersectCloudSphere(rayDir, cloudsData.cloudLayerMax, cloudsData.earthRadius);
    if (tEntry < 0.0 || tEntry > tExit)
        return result;
    if (tEntry > sceneDist)
        return result;
    tExit = min(tExit, sceneDist);
    
    float stepSize = (tExit - tEntry) / cloudsData.maxSteps;
    float weightPerStep = cloudsData.absorptionCoeff * stepSize;
    float jitter = hash(rayDir + frac(Constants.time));
    //float jitter = hash(uv + float2(Constants.time * 60.0, Constants.time * 73.0));
    jitter -= 0.5;
    float t = tEntry + jitter * stepSize;        
    float totalDensity = 0.0;
    
    for (uint step = 0; step < cloudsData.maxSteps; ++step)
    {
        float3 pos = rayOrigin + rayDir * t;
        float altitude = length(pos) - cloudsData.earthRadius;
        float norY = saturate((altitude - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness);

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsData);
        if (density > 0.0)
        {
            float absorption = exp(-density * stepSize * cloudsData.absorptionCoeff);
            float lightEnergy = VolumetricShadow(pos, cloudsTexture, cloudsData);
            lightEnergy += 0.2; // ambient

            float3 S = lightEnergy * density;
            float dTrans = absorption;
            float3 Sint = (S - S * dTrans) * (1.0f / max(density, 0.0001f)); // analytical integral
            
            result.color += result.transmittance * Sint;
            result.transmittance *= dTrans;
            result.nearDistance = min(result.nearDistance, t);
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
    Texture3D<float4> baseTexture = ResourceDescriptorHeap[cloudsData.cloudBaseShapeTexture];
    Texture2D<float> linearDepthTex = ResourceDescriptorHeap[cloudsData.linearDepthTexDI];
    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos); // homogeneous ray direction
    float3 rayDir = normalize(rayDirH.xyz);

    float viewZ = linearDepthTex.SampleLevel(pointClampSampler, input.uv, 0.0);
    float cosAngle = dot(rayDir, cloudsData.cameraForward);
    float sceneDist = viewZ / max(cosAngle, 0.0001);
        
    CloudResult clouds = GetCloudsColorRayMarch(
        Constants.cameraPosition, rayDir, baseTexture, cloudsData, sceneDist, input.position.xy);
    
    // hit in world space
    float3 hitPosWorld = Constants.cameraPosition + rayDir * clouds.nearDistance;
    //float3 hitPosWorld = Constants.cameraPosition + rayDir * clouds.weightedDistance;
    // hit in prev frame
    float4 prevClipPos = mul(cloudsData.matPrevFrameViewProj, float4(hitPosWorld, 1.0));
    prevClipPos.xyz /= prevClipPos.w;
    // Convert NDC to uv
    float2 prevUv = prevClipPos.xy * float2(0.5, -0.5) + 0.5;
    bool prevUvValid = all(prevUv > 0.0) && all(prevUv < 1.0) && prevClipPos.w > 0.0;
    //bool hasHit = clouds.nearDistance < (cloudsData.earthRadius * 2.0);
    bool hasHit = clouds.transmittance < 0.99;
    
    float4 currentColor = float4(clouds.color, clouds.transmittance);
    float4 finalColor = currentColor;
    
    //if (prevUvValid && hasHit)
    {
        Texture2D<float4> prevCloudsTex = ResourceDescriptorHeap[cloudsData.prevCloudsTexDI];
        float4 prevColor = prevCloudsTex.SampleLevel(linearClampSampler, prevUv, 0.0);
        
        float colorDiff = length(prevColor.rgb - currentColor.rgb);
        float alphaDiff = abs(prevColor.a - currentColor.a);
        float totalDiff = colorDiff + alphaDiff * 2.0;
        float blendFactor = saturate(0.05 + totalDiff * 2.0);
        //float blendFactor = 0.05;
        
        finalColor = lerp(prevColor, currentColor, blendFactor);
    }
    
    return finalColor;
    
#if 0
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

    float3 skyGradient = lerp(skyHorizon, skyZenith, pow(saturate(rayDir.y), 0.5));
    //skyGradient = lerp(skyGradient, sunColor, sunScatter * 0.35f);

    float distFade = 1.0 - saturate(clouds.weightedDistance / cloudsData.cloudFadeDistance);
    float3 cloudColor = clouds.color * float3(1.0f, 0.95f, 0.9f);
    float3 color = skyGradient * clouds.transmittance + cloudColor;
    color = lerp(skyGradient, color, distFade);
            
    return float4(color, clouds.transmittance);
#endif
}