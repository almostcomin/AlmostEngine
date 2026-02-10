#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::GenMipsConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> srcMip = ResourceDescriptorHeap[Constants.srcMipDI];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[Constants.dstMipDI];
    
    uint2 dstSize;
    dstMip.GetDimensions(dstSize.x, dstSize.y);
    
    if (any(DTid >= dstSize))
        return;
    
    uint2 srcPos = DTid * 2;
    
    float3 n0 = srcMip[srcPos + uint2(0, 0)].xyz;
    float3 n1 = srcMip[srcPos + uint2(1, 0)].xyz;
    float3 n2 = srcMip[srcPos + uint2(0, 1)].xyz;
    float3 n3 = srcMip[srcPos + uint2(1, 1)].xyz;
   
    // Map [0,1] to [-1,1]
    n0 = n0 * 2.0 - 1.0;
    n1 = n1 * 2.0 - 1.0;
    n2 = n2 * 2.0 - 1.0;
    n3 = n3 * 2.0 - 1.0;
    
    // Average the normals
    float3 avgNormal = (n0 + n1 + n2 + n3) * 0.25;
    
    // Renormalize
    avgNormal = normalize(avgNormal);
    
    // Map back [-1,1] to [0,1]
    avgNormal = avgNormal * 0.5 + 0.5;
    
    dstMip[DTid] = float4(avgNormal, 1.0);
}