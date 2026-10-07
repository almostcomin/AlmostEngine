#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

float3 ACESFilm(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

ConstantBuffer<interop::TonemapConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[Constants.InputColorTextureDI];
    Texture2D<float> exposureRatioTexture = ResourceDescriptorHeap[Constants.InputExposureRatioTextureDI];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[Constants.OutputTextureDI];
    
    if (any(DTid.xy > Constants.TextureDims))
        return;
            
    float4 color = inputTexture[DTid.xy];
    color *= exposureRatioTexture[uint2(0, 0)];
    color.rgb = ACESFilm(color.rgb);
        
    outputTexture[DTid.xy] = color;
}