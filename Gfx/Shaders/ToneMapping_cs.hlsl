#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::TonemapConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[Constants.outputTextureDI];
    
    uint2 outputSize;
    outputTexture.GetDimensions(outputSize.x, outputSize.y);
    
    if (DTid.x >= outputSize.x || DTid.y >= outputSize.y)
        return;
    
    float4 color = inputTexture[DTid.xy];
    
    color.rgb *= Constants.exposure;
    
    outputTexture[DTid.xy] = color;
}