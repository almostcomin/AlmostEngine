#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

ConstantBuffer<interop::BloomDownsampleConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float3 main(PS_INPUT input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[Constants.inputTextureDI];
    float x = Constants.invInputTexResolution.x;
    float y = Constants.invInputTexResolution.y;
    float2 uv = input.uv;
    
    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i    
    // === ('e' is the current texel) ===
    float3 a = texture.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y - 2 * y), 0).rgb;
    float3 b = texture.SampleLevel(linearClampSampler, float2(uv.x, uv.y - 2 * y), 0).rgb;
    float3 c = texture.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y - 2 * y), 0).rgb;
    
    float3 d = texture.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y), 0).rgb;
    float3 e = texture.SampleLevel(linearClampSampler, float2(uv.x, uv.y), 0).rgb;
    float3 f = texture.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y), 0).rgb;
    
    float3 g = texture.SampleLevel(linearClampSampler, float2(uv.x - 2 * x, uv.y + 2 * y), 0).rgb;
    float3 h = texture.SampleLevel(linearClampSampler, float2(uv.x, uv.y + 2 * y), 0).rgb;
    float3 i = texture.SampleLevel(linearClampSampler, float2(uv.x + 2 * x, uv.y + 2 * y), 0).rgb;
    
    float3 j = texture.SampleLevel(linearClampSampler, float2(uv.x - 1 * x, uv.y - 1 * y), 0).rgb;
    float3 k = texture.SampleLevel(linearClampSampler, float2(uv.x + 1 * x, uv.y - 1 * y), 0).rgb;
    float3 l = texture.SampleLevel(linearClampSampler, float2(uv.x - 1 * x, uv.y + 1 * y), 0).rgb;
    float3 m = texture.SampleLevel(linearClampSampler, float2(uv.x + 1 * x, uv.y + 1 * y), 0).rgb;
    
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

    return max(downsample, 1e-6);
}