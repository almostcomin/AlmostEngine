#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::DepthPrepassStageConstants> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
#if ALPHA_TEST
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
#endif    
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID, uint startInstance : SV_StartInstanceLocation)
{
    VS_OUTPUT output;
    
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    ByteAddressBuffer instancesIndexBuffer = ResourceDescriptorHeap[StageConstants.instancesDI];
    StructuredBuffer<interop::VisibleInstancePayload> payloadBuffer = ResourceDescriptorHeap[StageConstants.payloadDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
#if GPU_CULL
    interop::VisibleInstancePayload vp = payloadBuffer[startInstance + instanceID];
    interop::InstanceData instanceData = instancesDataBuffer[vp.InstanceIndex];
    interop::MeshData meshData = meshesDataBuffer[vp.MeshIndex];
#if ALPHA_TEST    
    output.materialIndex = vp.MaterialIndex;
#endif
#else
    uint actualInstanceId = instanceID + DrawConstants.baseInstanceIdx;
    uint instanceIndex = instancesIndexBuffer.Load(actualInstanceId * 4);
    interop::InstanceData instanceData = instancesDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
#if ALPHA_TEST
    output.materialIndex = DrawConstants.materialIndex;
#endif
#endif
    
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
        
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);

#if ALPHA_TEST    
    float2 uv0 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Offset);
    output.uv = uv0;    
#endif    
    
    output.pos = posClip;
    return output;
}
