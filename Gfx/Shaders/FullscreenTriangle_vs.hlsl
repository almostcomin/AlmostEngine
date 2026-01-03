#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT o;

    // Fullscreen triangle (no VB)
    float2 pos;
    pos.x = (vertexID == 2) ? 3.0f : -1.0f;
    pos.y = (vertexID == 1) ? -3.0f : 1.0f;
    
    o.position = float4(pos, 0.0, 1.0);
    o.uv = float2((pos.x + 1.0) * 0.5, (1.0f - pos.y) * 0.5);
    
    return o;
}