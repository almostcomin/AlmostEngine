#include "BindlessRS.hlsli"

struct PS_INPUT
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    return input.color;
};