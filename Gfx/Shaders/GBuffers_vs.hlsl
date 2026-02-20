#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SingleInstanceDrawData> Constants : register(b0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)    
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
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
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
        
    // Normal
    const float3x3 normalMatrix = (float3x3)transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normalMatrix, normal));
    float3 normalView = normalize(mul((float3x3)sceneData.camViewMatrix, normalWorld));
    
    // Tangent
    float3 tangentWorld = normalize(mul(instanceData.modelMatrix, float4(tangent.xyz, 0.0)).xyz);
    float3 tangentView = normalize(mul((float3x3)sceneData.camViewMatrix, tangentWorld));
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    output.normal = normalView;
    output.tangent = float4(tangentView, tangent.w);
    output.uv = uv0;
    return output;
}
