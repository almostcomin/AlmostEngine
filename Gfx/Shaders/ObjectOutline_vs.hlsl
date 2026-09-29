#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::ObjectOutlineConstants> StageConstants : register(b0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.SceneDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesDataBuffer[StageConstants.InstanceIndex];
    interop::MeshData meshData = meshesDataBuffer[StageConstants.MeshIndex];
    
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);

    float3 normal = Unpack_RGB8_SNORM(LoadVertexAttributeUInt(vertexBuffer, vertexBufferOffset, meshData.vertexNormalOffset));
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
        
    const float3x3 normalMatrix = (float3x3)transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normalMatrix, normal));
    float3 clipNormal = mul(sceneData.camViewProjMatrix, float4(normalWorld, 0)).xyz;
    
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);

    posClip.xy += normalize(clipNormal.xy + 1e-6.xx) * (StageConstants.ThicknessPx * 2.0 / StageConstants.ViewportHeight) * posClip.w;
    
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    return output;
}
