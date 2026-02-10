#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Color.hlsli"

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
        
    // Read the 4 texels
    uint2 srcPos = DTid * 2;
    float4 c0 = srcMip[srcPos + uint2(0, 0)];
    float4 c1 = srcMip[srcPos + uint2(1, 0)];
    float4 c2 = srcMip[srcPos + uint2(0, 1)];
    float4 c3 = srcMip[srcPos + uint2(1, 1)];
    
    // Converts to linear
    c0.rgb = SRGBToLinear(c0.rgb);
    c1.rgb = SRGBToLinear(c1.rgb);
    c2.rgb = SRGBToLinear(c2.rgb);
    c3.rgb = SRGBToLinear(c3.rgb);
    
    // Average
    float4 avgColor = (c0 + c1 + c2 + c3) * 0.25;
    
    // Back to SRGB
    avgColor.rgb = LinearToSRGB(avgColor.rgb);
    
    dstMip[DTid] = avgColor;
}