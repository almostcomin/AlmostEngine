#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::BlitComputeConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> srcTexture = ResourceDescriptorHeap[Constants.srcTextureDI];
    RWTexture2D<float4> dstTexture = ResourceDescriptorHeap[Constants.dstTextureDI];
    
    uint2 pixelPos = DTid.xy + Constants.viewBegin.xy;    
    if (all(pixelPos.xy < Constants.viewEnd.xy))
    {
        float4 color = srcTexture[pixelPos];
        dstTexture[pixelPos] = color;
    }
}