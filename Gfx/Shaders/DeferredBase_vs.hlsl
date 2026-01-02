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
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = indexBuffer[vertexID + meshData.indexOffset];
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttributeFloat3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    float3 normal = DecodeSnorm8(LoadVertexAttributeUInt(vertexBuffer, vertexBufferOffset, meshData.vertexNormalOffset));
    float2 uv0 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Offset);
        
    // Position
    float4x4 modelMatrix = instanceData.modelMatrix;
    float4 posWorld = mul(float4(pos, 1.0f), modelMatrix);
    float4x4 viewProjectionMatrix = sceneData.viewProjectionMatrix;
    float4 posClip = mul(posWorld, viewProjectionMatrix);
    // Normal
    const float3x3 normalMatrix = (float3x3) transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normal, normalMatrix));
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    output.normal = normalWorld;
    output.uv = uv0;
    return output;
}
