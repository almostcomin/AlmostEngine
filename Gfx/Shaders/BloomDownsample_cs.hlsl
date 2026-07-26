#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

#define GROUP_SIZE 16

ConstantBuffer<interop::BloomDownsampleConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= Constants.outputTexResolution))
        return;
        
    Texture2D inputTex = ResourceDescriptorHeap[Constants.inputTextureDI];
    float x = Constants.inputTexInvResolution.x;
    float y = Constants.inputTexInvResolution.y;
    
    // Assuming outputResdolution = 2 * inputResolution
    float2 uv = (DTid + 0.5) * Constants.inputTexInvResolution * 2.0;
        
    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i    
    // === ('e' is the current texel) ===
    float3 a = inputTex.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y - 2 * y), 0).rgb;
    float3 b = inputTex.SampleLevel(linearClampSampler, float2(uv.x,         uv.y - 2 * y), 0).rgb;
    float3 c = inputTex.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y - 2 * y), 0).rgb;
    
    float3 d = inputTex.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y),         0).rgb;
    float3 e = inputTex.SampleLevel(linearClampSampler, float2(uv.x,         uv.y),         0).rgb;
    float3 f = inputTex.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y),         0).rgb;
    
    float3 g = inputTex.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y + 2 * y), 0).rgb;
    float3 h = inputTex.SampleLevel(linearClampSampler, float2(uv.x,         uv.y + 2 * y), 0).rgb;
    float3 i = inputTex.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y + 2 * y), 0).rgb;
    
    float3 j = inputTex.SampleLevel(linearClampSampler, float2(uv.x - 1 * x, uv.y - 1 * y), 0).rgb;
    float3 k = inputTex.SampleLevel(linearClampSampler, float2(uv.x + 1 * x, uv.y - 1 * y), 0).rgb;
    float3 l = inputTex.SampleLevel(linearClampSampler, float2(uv.x - 1 * x, uv.y + 1 * y), 0).rgb;
    float3 m = inputTex.SampleLevel(linearClampSampler, float2(uv.x + 1 * x, uv.y + 1 * y), 0).rgb;
    
    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    float3 downsample = e * 0.125;
    downsample += (a + c + g + i) * 0.03125;
    downsample += (b + d + f + h) * 0.0625;
    downsample += (j + k + l + m) * 0.125;

    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    //outputTex[DTid] = float4(max(downsample, 1e-6), 1.0);
    outputTex[DTid] = float4(downsample, 1.0);
}