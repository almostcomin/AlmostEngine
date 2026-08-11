#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "CloudsCommon.hlsli"

ConstantBuffer<interop::CloudsShadowmapConstants> Constants : register(b0);

// Returns transmittance
float ComputeShadowTransmittance(float3 rayOriginLocal, float3 rayDir, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer<interop::CloudsShapeData> cloudsShape, ConstantBuffer<interop::CloudsShadowmapData> shadowmapData, float sceneDist, float2 pixelPos)
{
    // Translate ray origin to Earth-centered coordinates.
    float3 rayOrigin = rayOriginLocal - cloudsShape.EarthCenter;
    
    // Distance to hit in solid geometry
    float3 hitWorld = shadowmapData.SunPos + rayDir * sceneDist;
    float3 hitEarthLocal = hitWorld - cloudsShape.EarthCenter;
    float hitDist = length(hitEarthLocal - rayOrigin);
    
    float tEntry, tExit;
    if (!GetCloudsLayerIntersectionPoints(rayOrigin, rayDir, cloudsShape.EarthRadius,
        cloudsShape.EarthRadius + cloudsShape.CloudLayerMinH, cloudsShape.EarthRadius + cloudsShape.CloudLayerMaxH,
        hitDist, tEntry, tExit))
    {
        return 1.0; // Ray misses shell entirely (or fully behind geometry)
    }
    float marchExit = tExit; 
    float rayLength = marchExit - tEntry;
    
    // Adaptive step count, clamped to shadowmapData.lightSteps
    // (matches the pattern in GetCloudsColorRayMarch)
    //const float targetStepSize = 100.0; // meters, tune to your resolution
    //uint effectiveSteps = (uint)ceil(rayLength / max(targetStepSize, 0.001));
    //effectiveSteps = clamp(effectiveSteps, shadowmapData.lightSteps / 2, shadowmapData.lightSteps);
    uint effectiveSteps = shadowmapData.RayMarchStepCount;
    
    float stepSize = rayLength / max(effectiveSteps, 1u);
    float transmittance = 1.0;
    float t = tEntry;
    
    for (uint step = 0; step < effectiveSteps; ++step)
    {
        // Spatial jitter only (no temporal: ping-pong off for now)
        float stepJ = StepJitter(pixelPos, step, 0.0);
        float sampleT = t + stepJ * stepSize;

        float3 pos = rayOrigin + rayDir * sampleT;
        float altitude = length(pos) - cloudsShape.EarthRadius;
        float norY = (altitude - cloudsShape.CloudLayerMinH) * cloudsShape.InvCloudLayerThickness;

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsDetailTexture, cloudsShape);
        if(density > 0.0)
        {
            float segmentTransmittance = exp(-density * stepSize * cloudsShape.muT);
            transmittance *= segmentTransmittance;
            if (transmittance < 0.001)
                break;
        }
        
        t += stepSize;
    }

    return transmittance;
}

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    ConstantBuffer<interop::CloudsShapeData> cloudsShape = ResourceDescriptorHeap[Constants.CloudsShapeDataDI];
    ConstantBuffer<interop::CloudsShadowmapData> cloudsShadowmapData = ResourceDescriptorHeap[Constants.CloudsShadowmapDataDI];    
        
    if (any(DTid >= cloudsShadowmapData.DstTextureSize))
        return;
    
    Texture3D<float4> baseTexture = ResourceDescriptorHeap[cloudsShape.BaseShapeTexture];
    Texture3D<float4> detailTexture = ResourceDescriptorHeap[cloudsShape.DetailTexture];
    Texture2D<float> linearDepthTex = ResourceDescriptorHeap[cloudsShadowmapData.LinearDepthTexDI];
    RWTexture2D<float> dstTexture = ResourceDescriptorHeap[cloudsShadowmapData.DstTextureDI];
    
    float2 uv = (float2(DTid) + 0.5) / float2(cloudsShadowmapData.DstTextureSize);
    
    float4 clipPosNear;
    clipPosNear.x = uv.x * 2.0 - 1.0;
    clipPosNear.y = 1.0 - uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPosNear.z = 1.0;              // Near plane
    clipPosNear.w = 1.0;
    
    float4 offsetH = mul(cloudsShadowmapData.MatClipToTranslatedWorld, clipPosNear);
    float3 offset = offsetH.xyz; // Ortographic & near = 0, w=1    
    
    float3 startPos = cloudsShadowmapData.SunPos + offset;    
    float3 rayDir = cloudsShadowmapData.SunDir;

    float transmittance = ComputeShadowTransmittance(
        startPos, rayDir, baseTexture, detailTexture, cloudsShape, cloudsShadowmapData, INFINITE_DEPTH, float2(DTid.xy));
    
    dstTexture[DTid] = transmittance;
}