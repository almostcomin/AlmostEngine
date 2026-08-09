#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::CloudsShadowmapConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    ConstantBuffer<interop::CloudsShadowmapData> cloudsShadowmapData = ResourceDescriptorHeap[Constants.CloudsShadowmapDataDI];    
        
    if (any(DTid >= cloudsShadowmapData.DstTextureSize))
        return;
    
    Texture2D<float> linearDepthTex = ResourceDescriptorHeap[cloudsShadowmapData.LinearDepthTexDI];
    RWTexture2D<float> dstTexture = ResourceDescriptorHeap[cloudsShadowmapData.DstTextureDI];
    
    float2 uv = (float2(DTid) + 0.5) / float2(cloudsShadowmapData.DstTextureSize);
    
    float4 clipPosNear;
    clipPosNear.x = uv.x * 2.0 - 1.0;
    clipPosNear.y = 1.0 - uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPosNear.z = 0.0; // Far plane
    clipPosNear.w = 1.0;
    
    float4 offsetH = mul(cloudsShadowmapData.MatClipToTranslatedWorld, clipPosNear);
    float3 offset = offsetH.xyz; // Ortographic & near = 0, w=1    
    
    float3 startPos = cloudsShadowmapData.SunPos + offset;    
    float3 rayDir = cloudsShadowmapData.SunDir;
        
//    CloudResult clouds = GetCloudsColorRayMarch(
//        Constants.cameraPosition, rayDir, baseTexture, detailTexture, cloudsShape, cloudsData, sceneDist, pixelPos);
    
    dstTexture[DTid] = -rayDir.y;
}