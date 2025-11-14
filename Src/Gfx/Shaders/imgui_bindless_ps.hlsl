#include "Interop/ImGUI_CB.h"

ConstantBuffer<interop::ImGUI_CB> renderResources : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[renderResources.textureIndex];
    
    float4 out_col = input.col * texture0.Sample(pointWrapSampler, input.uv);
    return out_col;
}
