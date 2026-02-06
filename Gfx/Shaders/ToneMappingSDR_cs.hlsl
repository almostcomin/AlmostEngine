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
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[Constants.inputTextureDI];
    Texture2D<float> avgLuminanceTexture = ResourceDescriptorHeap[Constants.inputAvgLuminanceTextureDI];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[Constants.outputTextureDI];
    
    uint2 outputSize;
    outputTexture.GetDimensions(outputSize.x, outputSize.y);
    
    if (DTid.x >= outputSize.x || DTid.y >= outputSize.y)
        return;
    
    float avgLuminance = avgLuminanceTexture[uint2(0, 0)];
    float exposure = Constants.middleGray / max(avgLuminance, 0.001);
    exposure *= Constants.sdrExposureBias;
    
    float4 color = inputTexture[DTid.xy];
    color.rgb *= exposure;
    color.rgb = ACESFilm(color.rgb);
        
    outputTexture[DTid.xy] = color;
}