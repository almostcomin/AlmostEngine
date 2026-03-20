#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::WBOITAccumStageConstants> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
    float3 posView : TEXCOORD1;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    ByteAddressBuffer instancesIndexBuffer = ResourceDescriptorHeap[StageConstants.instancesDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
    uint actualInstanceId = instanceID + DrawConstants.baseInstanceIdx;
    uint instanceIndex = instancesIndexBuffer.Load(actualInstanceId * 4);
    interop::InstanceData instanceData = instancesDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
             
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    float3 normal = Unpack_RGB8_SNORM(LoadVertexAttributeUInt(vertexBuffer, vertexBufferOffset, meshData.vertexNormalOffset));
    float4 tangent = Unpack_RGBA8_SNORM(LoadVertexAttributeUInt(vertexBuffer, vertexBufferOffset, meshData.vertexTangetOffset));
    float2 uv0 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Offset);
        
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float3 posView = mul(sceneData.camViewMatrix, posWorld).xyz;
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
        
    // Normal
    const float3x3 normalMatrix = (float3x3) transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normalMatrix, normal));
    float3 normalView = normalize(mul((float3x3) sceneData.camViewMatrix, normalWorld));
    
    // Tangent
    float3 tangentWorld = normalize(mul(instanceData.modelMatrix, float4(tangent.xyz, 0.0)).xyz);
    float3 tangentView = normalize(mul((float3x3) sceneData.camViewMatrix, tangentWorld));
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    output.normal = normalView;
    output.tangent = float4(tangentView, tangent.w);
    output.uv = uv0;
    output.posView = posView;
    return output;
}
