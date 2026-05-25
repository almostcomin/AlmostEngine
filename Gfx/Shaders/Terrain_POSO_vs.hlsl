#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::DepthPrepassStageConstants> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{    
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    StructuredBuffer<interop::HeightmapPatchData> patchDataBuffer = ResourceDescriptorHeap[sceneData.patchDataBufferDI];
    
    uint instanceIndex = instanceID + DrawConstants.rawInstanceBaseIdx;
    interop::InstanceData instanceData = instancesDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
             
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Patch data
    uint patchIndex = instanceID + DrawConstants.extraDataBaseIdx;
    interop::HeightmapPatchData patchData = patchDataBuffer[patchIndex];
    Texture2D<float> heightsTexture = ResourceDescriptorHeap[patchData.HeightmapTextureDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    // Build local pos
    float2 pos2 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    float2 uv = patchData.MinUV + pos2 * patchData.CellSize;
    uv /= patchData.DataNormSize;
    float H = heightsTexture.SampleLevel(linearClampSampler, uv, 0).r;
    
    float3 pos = float3(pos2.x, H, pos2.y);
        
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
    
    // Output
    VS_OUTPUT output;
    output.pos = posClip;

    return output;
}
