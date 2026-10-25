#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::GenMipsConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    Texture2DArray<float4> srcMip = ResourceDescriptorHeap[Constants.srcMipDI];
    RWTexture2DArray<float4> dstMip = ResourceDescriptorHeap[Constants.dstMipDI];
    
    uint2 dstSize;
    uint dstDepth;
    dstMip.GetDimensions(dstSize.x, dstSize.y, dstDepth);
    
    if (any(DTid >= dstSize))
        return;
    
    // Read the 4 texels
    uint2 srcPos = DTid * 2;
    float4 c0 = srcMip[uint3(srcPos + uint2(0, 0), 0)];
    float4 c1 = srcMip[uint3(srcPos + uint2(1, 0), 0)];
    float4 c2 = srcMip[uint3(srcPos + uint2(0, 1), 0)];
    float4 c3 = srcMip[uint3(srcPos + uint2(1, 1), 0)];
    
    // Average
    float4 avgColor = (c0 + c1 + c2 + c3) * 0.25;
    
    dstMip[uint3(DTid, 0)] = avgColor;
}