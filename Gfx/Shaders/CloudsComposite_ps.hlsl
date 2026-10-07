#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::CloudsCompositeConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    Texture2D cloudsTexture = ResourceDescriptorHeap[Constants.CloudsTextureDI];
    
    float4 color = cloudsTexture.Sample(linearClampSampler, input.uv);
    color.rgb = ApplyExposure(color.rgb, Constants.ExposureFactorTextureDI);
    
    return color;
}