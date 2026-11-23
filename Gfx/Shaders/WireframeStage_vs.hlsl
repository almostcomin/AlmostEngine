// ALM_REQUIRE_VARIANT(GPU_CULL)
// ALM_REQUIRE_SM(6.8; GPU_CULL)

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::WireframeStageConstats> StageConstants : register(b0);
ConstantBuffer<interop::MeshPassProcessorConstants> DrawConstants : register(b1);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID
#if GPU_CULL
    , uint startInstance : SV_StartInstanceLocation
#endif
)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::InstanceTransformData> instancesTransformDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
        
#if GPU_CULL
    StructuredBuffer<interop::InstancePayload> payloadBuffer = ResourceDescriptorHeap[StageConstants.payloadDI];
    
    interop::InstancePayload vp = payloadBuffer[startInstance + instanceID];
    interop::InstanceTransformData instanceTransformData = instancesTransformDataBuffer[vp.InstanceIndex];
    interop::MeshData meshData = meshesDataBuffer[vp.MeshIndex];    
#else
    ByteAddressBuffer instancesIndexBuffer = ResourceDescriptorHeap[StageConstants.instancesDI];
    
    uint actualInstanceId = instanceID + DrawConstants.baseInstanceIdx;
    uint instanceIndex = instancesIndexBuffer.Load(actualInstanceId * 4);
    interop::InstanceTransformData instanceTransformData = instancesTransformDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
#endif
    
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
        
    // Transform
    float4 posWorld = mul(instanceTransformData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    return output;
}
