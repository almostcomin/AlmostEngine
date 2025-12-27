#include "Interop/ImGUI_CB.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ImGUI_CB> CB : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[CB.textureIndex];
    
    float4 out_col = input.col * texture.Sample(pointWrapSampler, input.uv);
    return out_col;
}
