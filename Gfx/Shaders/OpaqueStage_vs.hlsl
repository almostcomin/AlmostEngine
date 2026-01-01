#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::OpaqueStage> Constants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
PS_INPUT main(uint vertexID : SV_VertexID)
{
    uint sceneDI = Constants.sceneDI;
    uint instanceBufferDI = Constants.instanceBufferDI;
    uint meshesBufferDI = Constants.meshesBufferDI;
    uint instanceIdx = Constants.instanceIdx;
    
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Fetch vertex data
    uint baseIndex = indexBuffer[vertexID + meshData.indexOffset];
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    float2 uv0 = LoadVertexAttribute2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Offset);
        
    // Transform
    float4x4 modelMatrix = instanceData.modelMatrix;
    float4 posWorld = mul(float4(pos, 1.0f), modelMatrix);
    float4x4 viewProjectionMatrix = sceneData.viewProjectionMatrix;
    float4 posClip = mul(posWorld, viewProjectionMatrix);
    //const float3x3 normalMatrix = (float3x3) transpose(instanceData.inverseModelMatrix);
    //float3 normalWorld = normalize(mul(normal, normalMatrix));
        
    // Output
    PS_INPUT output;
    output.pos = posClip;
    output.normal = float3(0.0, 0.0, 0.0);//normalWorld;
    output.uv = uv0;
    return output;
}
