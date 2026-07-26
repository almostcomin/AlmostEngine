#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

#define GROUP_SIZE 16

ConstantBuffer<interop::BloomUpsampleConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= Constants.outputTexResolution))
        return;
    
    Texture2D texture = ResourceDescriptorHeap[Constants.inputTextureDI];
    
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = Constants.filterRadius;
    float y = Constants.filterRadius;
    
    float2 uv = (DTid + 0.5) * Constants.outputTexInvResolution;
    
    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = texture.SampleLevel(linearClampSampler, float2(uv.x - x, uv.y - y), 0).rgb;
    float3 b = texture.SampleLevel(linearClampSampler, float2(uv.x,     uv.y - y), 0).rgb;
    float3 c = texture.SampleLevel(linearClampSampler, float2(uv.x + x, uv.y - y), 0).rgb;
    
    float3 d = texture.SampleLevel(linearClampSampler, float2(uv.x - x, uv.y), 0).rgb;
    float3 e = texture.SampleLevel(linearClampSampler, float2(uv.x,     uv.y), 0).rgb;
    float3 f = texture.SampleLevel(linearClampSampler, float2(uv.x + x, uv.y), 0).rgb;
    
    float3 g = texture.SampleLevel(linearClampSampler, float2(uv.x - x, uv.y + y), 0).rgb;
    float3 h = texture.SampleLevel(linearClampSampler, float2(uv.x,     uv.y + y), 0).rgb;
    float3 i = texture.SampleLevel(linearClampSampler, float2(uv.x + x, uv.y + y), 0).rgb;
    
    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;

    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    float4 currentC = outputTex[DTid];
    outputTex[DTid] = float4(currentC.rgb + upsample, 1.0);
}