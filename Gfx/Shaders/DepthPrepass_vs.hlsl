#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::MultiInstanceDrawConstants> Constants : register(b0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{    
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    ByteAddressBuffer instancesIndexBuffer = ResourceDescriptorHeap[Constants.instancesDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
    uint actualInstanceId = instanceID + Constants.baseInstanceIdx;
    uint instanceIndex = instancesIndexBuffer.Load(actualInstanceId * 4);
    interop::InstanceData instanceData = instancesDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[Constants.meshIndex];
             
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
        
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    return output;
}
