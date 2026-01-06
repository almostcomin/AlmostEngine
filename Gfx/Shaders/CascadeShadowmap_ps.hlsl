#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

struct PS_INPUT
{
    float4 position : SV_Position;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{    
    float4 color = float4(input.position.z, 0.0, 0.0, 1.0);
    return color;
}