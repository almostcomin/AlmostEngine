// clang-format off

#include "BindlessRS.hlsli"
#include "Interop/ConstantBuffers.h"
#include "Interop/RenderResources.h"

struct VSOutput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
};

ConstantBuffer<interop::TriangleRenderResources> renderResources : register(b0);

[RootSignature(BindlessRootSignature)] VSOutput main(uint vertexID: SV_VertexID) 
{
    StructuredBuffer<float3> positionBuffer = ResourceDescriptorHeap[renderResources.positionBufferIndex];
    StructuredBuffer<float2> textureCoordBuffer = ResourceDescriptorHeap[renderResources.textureCoordBufferIndex];

    ConstantBuffer<interop:: CameraCB> cameraCB = ResourceDescriptorHeap[renderResources.cameraBufferIndex];
    
    VSOutput output;

    output.position = mul(float4(positionBuffer[vertexID].xyz, 1.0f), cameraCB.viewProjectionMatrix);
    output.textureCoord = textureCoordBuffer[vertexID];

    return output;
}
