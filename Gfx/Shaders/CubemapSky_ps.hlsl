#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::CubemapSkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{    
    float4 clipPos;
    clipPos.x = input.uv.x * 2 - 1;
    clipPos.y = 1 - input.uv.y * 2;
    clipPos.z = 0.5;
    clipPos.w = 1;
    float4 translatedWorldPos = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 direction = normalize(translatedWorldPos.xyz / translatedWorldPos.w);
        
    TextureCube<float4> cubemap = ResourceDescriptorHeap[Constants.cubemapTextureDI];
    float3 color = cubemap.Sample(linearClampSampler, direction).rgb;
    
    color = ApplyExposure(color, Constants.exposureFactorTextureDI);
         
    return float4(color, 1.0);
}