#include "Interop/ImGUI_CB.h"

ConstantBuffer<interop::ImGUI_CB> renderResources : register(b0);

struct PS_INPUT
{
    float4 out_pos : SV_POSITION;
    float4 out_col : COLOR0;
    float2 out_uv  : TEXCOORD0;
};

PS_INPUT main(uint vertexID : SV_VertexID)
{
    StructuredBuffer<float3> positionBuffer = ResourceDescriptorHeap[renderResources.positionBufferIndex];
    StructuredBuffer<float4> colorBuffer = ResourceDescriptorHeap[renderResources.colorBufferIndex];
    StructuredBuffer<float2> textureCoordBuffer = ResourceDescriptorHeap[renderResources.textureCoordBufferIndex];
    
    PS_INPUT output;
    
    // Transform to NDC
    output.out_pos.xy = positionBuffer[vertexID].xy * g_Const.invDisplaySize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.out_pos.zw = float2(0, 1);
    
    output.out_col = colorBuffer[vertexID];
    output.out_uv = textureCoordBuffer[vertexID];
    
    return output;
}

