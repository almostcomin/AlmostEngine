#include "Interop/ImGUI_CB.h"

ConstantBuffer<interop::ImGUI_CB> renderResources : register(b0);

struct PS_INPUT
{
    float4 out_pos : SV_POSITION;
    float4 out_col : COLOR0;
    float2 out_uv  : TEXCOORD0;
};

struct ImDrawVert
{
    float2 pos;
    float2 uv;
    uint color; // RGBA8 packed
};

float4 UnpackColor(uint v)
{
    return float4(
        (v.col & 0xFF) / 255.0,
        ((v.col >> 8) & 0xFF) / 255.0,
        ((v.col >> 16) & 0xFF) / 255.0,
        ((v.col >> 24) & 0xFF) / 255.0);
};

PS_INPUT main(uint vertexID : SV_VertexID)
{
    StructuredBuffer<uint> indexBuffer = ResourceDescriptorHeap[renderResources.indexBuffer];
    StructuredBuffer<ImDrawVert> vertexBuffer = ResourceDescriptorHeap[renderResources.vertexBuffer];
    
    PS_INPUT output;
    
    uint baseIndex = indexBuffer[vertexID + renderResources.indexOffset];
    ImDrawVert v = vertexBuffer[baseIndex + renderResources.positionBufferOffset];
    
    // Transform to NDC
    output.out_pos.xy = v.pos.xy * g_Const.invDisplaySize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.out_pos.zw = float2(0, 1);
    
    output.out_col = UnpackColor[v.color];
    output.out_uv = textureCoordBuffer[v.uv];
    
    return output;
}

