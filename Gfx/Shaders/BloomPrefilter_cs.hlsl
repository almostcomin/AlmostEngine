#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Bloom prefilter based on https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#define GROUP_SIZE 16

ConstantBuffer<interop::BloomPrefilterConstants> Constants : register(b0);

float3 KneeThreshold(float3 color, float threshold, float knee)
{    
    float brightness = max(max(color.r, color.g), color.b);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft * (0.25 / (knee + 1e-5));
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 1e-5);
    float3 thresholded = color * contribution;
    
    return thresholded;
}

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= Constants.texResolution))
        return;
        
    Texture2D inputTex = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    
    float2 uv = (DTid + 0.5) * Constants.invTexResolution;
    
    float3 srcColor = inputTex.SampleLevel(pointClampSampler, uv, 0).rgb;
    float3 dstColor = KneeThreshold(srcColor, Constants.threshold, Constants.knee);
    
    outputTex[DTid] = float4(dstColor, 1.0);
}