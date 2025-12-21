#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::BlitConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[Constants.textureDI];
    
    float4 color = texture.Sample(pointClampSampler, input.uv);
    
    return color;
}