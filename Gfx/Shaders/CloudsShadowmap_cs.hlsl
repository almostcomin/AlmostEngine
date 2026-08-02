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
    
    float2 uv = DTid / float2(cloudsShadowmapData.DstTextureSize);
    
    // Reconstruct world-space ray direction from clip-space coordinates.
    // matClipToTranslatedWorld transforms from clip space to world space
    // with the camera at the origin (translated world), so no camera translation
    // is baked into the matrix -- avoids floating point precision issues at large distances.
    float4 clipPos;
    clipPos.x = uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 rayDirH = mul(cloudsShadowmapData.MatClipToTranslatedWorld, clipPos);
    float3 rayDir = normalize(rayDirH.xyz);
    
    // Read linear depth (distance along the camera forward axis) and convert to scene distance along the ray direction.
    // viewZ: depth along camera forward vector (Z component in view space)
    // sceneDist: actual distance along rayDir to the geometry
    float viewZ = linearDepthTex.SampleLevel(linearClampSampler, uv, 0.0);
    float cosAngle = saturate(dot(rayDir, cloudsShadowmapData.CameraForward));
    float sceneDist = viewZ / max(cosAngle, 0.0001);
        
    dstTexture[DTid] = sceneDist / 100.0;
}