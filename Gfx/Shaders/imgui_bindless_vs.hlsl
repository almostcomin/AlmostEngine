#include "Interop/ImGUI_CB.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ImGUI_CB> CB : register(b0);

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
        (v & 0xFF) / 255.0,
        ((v >> 8) & 0xFF) / 255.0,
        ((v >> 16) & 0xFF) / 255.0,
        ((v >> 24) & 0xFF) / 255.0);
};

[RootSignature(BindlessRootSignature)]
PS_INPUT main(uint vertexID : SV_VertexID)
{
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[CB.indexBuffer];
    StructuredBuffer<ImDrawVert> vertexBuffer = ResourceDescriptorHeap[CB.vertexBuffer];
    
    PS_INPUT output;
    
    uint idxOffset = CB.indexOffset;
    uint baseIndex = indexBuffer[vertexID + idxOffset];
    
    uint vertexOffset = CB.vertexBufferOffset;
    ImDrawVert v = vertexBuffer[baseIndex + vertexOffset];
    
    // Transform to NDC
    output.out_pos.xy = (v.pos.xy - CB.clipOffset) * CB.invDisplaySize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.out_pos.zw = float2(0, 1);
    
    output.out_col = UnpackColor(v.color);
    output.out_uv = v.uv;
    
    return output;
}

