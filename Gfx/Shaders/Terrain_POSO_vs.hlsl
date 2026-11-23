// ALM_REQUIRE_VARIANT(GPU_CULL, SHADOW_MATRIX)
// ALM_REQUIRE_SM(6.8; GPU_CULL)

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"
#include "HeightmapCommon.hlsli"

ConstantBuffer<interop::DepthPrepassStageConstants> StageConstants : register(b0);
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
    StructuredBuffer<interop::HeightmapPatchData> patchDataBuffer = ResourceDescriptorHeap[sceneData.patchDataBufferDI];

#if GPU_CULL
    StructuredBuffer<interop::InstancePayload> payloadBuffer = ResourceDescriptorHeap[StageConstants.payloadDI];
    
    interop::InstancePayload vp = payloadBuffer[startInstance + instanceID];
    interop::InstanceTransformData instanceTransformData = instancesTransformDataBuffer[vp.InstanceIndex];
    interop::MeshData meshData = meshesDataBuffer[vp.MeshIndex];    
    uint patchIndex = vp.ExtraDataBaseIdx;    
#else
    uint instanceIndex = instanceID + DrawConstants.rawInstanceBaseIdx;
    interop::InstanceTransformData instanceTransformData = instancesTransformDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
    uint patchIndex = instanceID + DrawConstants.extraDataBaseIdx;
#endif
    
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Patch data
    interop::HeightmapPatchData patchData = patchDataBuffer[patchIndex];
    Texture2D<float> heightsTexture = ResourceDescriptorHeap[patchData.HeightmapTextureDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    // Build local pos
    float2 pos2 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    float2 uv = patchData.MinUV + pos2 * patchData.SizeUV;
    uv *= patchData.UVScale;
    float H = heightsTexture.SampleLevel(linearClampSampler, uv, patchData.MipLevel + GetHeightmapMipBias(pos2, patchData.EdgeMask)).r;
    
    float3 pos = float3(pos2.x, H, pos2.y);
        
    // Transform
    float4 posWorld = mul(instanceTransformData.modelMatrix, float4(pos, 1.0f));
#if SHADOW_MATRIX
    float4 posClip = mul(sceneData.shadowMapWorldToClipMatrix, posWorld);
#else
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
#endif
    
    // Output
    VS_OUTPUT output;
    output.pos = posClip;

    return output;
}
