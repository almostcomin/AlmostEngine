#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

struct VS_OUTPUT
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

ConstantBuffer<interop::DebugStage> Constants : register(b0);

static const uint edgeIndices[24] =
{
    0, 1, 1, 2, 2, 3, 3, 0, // button face
    4, 5, 5, 6, 6, 7, 7, 4, // top face
    0, 4, 1, 5, 2, 6, 3, 7 // vertical lines
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::AABB> aabboxBuffer = ResourceDescriptorHeap[Constants.aaboxDI];
    
    interop::AABB box = aabboxBuffer[instanceID];
    
    // 8 esquinas del AABB
    float3 corners[8] =
    {
        float3(box.min.x, box.min.y, box.min.z),
        float3(box.max.x, box.min.y, box.min.z),
        float3(box.max.x, box.max.y, box.min.z),
        float3(box.min.x, box.max.y, box.min.z),

        float3(box.min.x, box.min.y, box.max.z),
        float3(box.max.x, box.min.y, box.max.z),
        float3(box.max.x, box.max.y, box.max.z),
        float3(box.min.x, box.max.y, box.max.z),
    };
    
    uint cornerIdx = edgeIndices[vertexID];
    float3 worldPos = corners[cornerIdx];
    
    VS_OUTPUT o;
    o.pos = mul(sceneData.camViewProjMatrix, float4(worldPos, 1.0));
    o.color = float4(1, 0, 0, 1);
    
    return o;
}