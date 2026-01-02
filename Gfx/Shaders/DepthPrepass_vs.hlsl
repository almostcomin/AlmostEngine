#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SingleInstanceDrawData> Constants : register(b0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID)
{    
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[Constants.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[Constants.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = indexBuffer[vertexID + meshData.indexOffset];
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
        
    // Transform
    float4x4 modelMatrix = instanceData.modelMatrix;
    float4 posWorld = mul(float4(pos, 1.0f), modelMatrix);
    float4x4 viewProjectionMatrix = sceneData.viewProjectionMatrix;
    float4 posClip = mul(posWorld, viewProjectionMatrix);

        
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    return output;
}
