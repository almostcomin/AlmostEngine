#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// Based on https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

ConstantBuffer<interop::BloomMixConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float3 main(PS_INPUT input) : SV_Target
{
    Texture2D sceneTexture = ResourceDescriptorHeap[Constants.sceneTextureDI];
    Texture2D bloomTexture = ResourceDescriptorHeap[Constants.bloomTextureDI];
    
    float3 hdrColor = sceneTexture.SampleLevel(linearClampSampler, input.uv, 0).rgb;
    float3 bloomColor = bloomTexture.SampleLevel(linearClampSampler, input.uv, 0).rgb;
    float3 finalColor = lerp(hdrColor, bloomColor, Constants.bloomStrength);
    
    return finalColor;
}