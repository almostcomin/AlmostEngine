#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

float3 HDRHighlightRolloff(float3 x, float knee, float maxNits)
{
    float3 result;
    result.r = x.r < knee ? x.r : knee + (x.r - knee) / (1.0 + (x.r - knee) / (maxNits - knee));
    result.g = x.g < knee ? x.g : knee + (x.g - knee) / (1.0 + (x.g - knee) / (maxNits - knee));
    result.b = x.b < knee ? x.b : knee + (x.b - knee) / (1.0 + (x.b - knee) / (maxNits - knee));
    return result;
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
    
    // 1. Exposure adjustment
    float avgLuminance = avgLuminanceTexture[uint2(0, 0)];
    // Constants.middleGray is a multiplier of paper white, typically around 0.18
    // For instance, if paper white is 200 nits, 0.18 * 200 = 36 nits.
    float exposureAdjustment = Constants.middleGray / max(avgLuminance, 0.001);
    float4 color = inputTexture[DTid.xy];
    color.rgb *= exposureAdjustment;
        
    // 2. Soft rolloff on highlights
    color.rgb = HDRHighlightRolloff(color.rgb, 5000.0, 10000.0);
        
    outputTexture[DTid.xy] = color;
}