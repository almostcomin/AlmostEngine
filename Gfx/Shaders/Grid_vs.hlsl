#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

struct VS_OUTPUT
{
    float4 pos : SV_Position;
    float3 worldPos : WORLDPOS;
    nointerpolation uint colorIdx : COLORIDX;
};

ConstantBuffer<interop::GridStageConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::GridVertex> gridVertices = ResourceDescriptorHeap[Constants.gridVerticesDI];

    interop::GridVertex vertex = gridVertices[vertexID];

    VS_OUTPUT o;
    o.pos = mul(sceneData.camViewProjMatrix, float4(vertex.pos, 1.0));
    o.worldPos = vertex.pos;
    o.colorIdx = vertex.colorIdx;
    return o;
}
