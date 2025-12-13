#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

float2 LoadVertexAttribute2(ByteAddressBuffer buffer, uint offset, uint stride)
{
    if (stride == 0xFFFFFFFF)
        return float2(0, 0);
    return asfloat(buffer.Load2(offset + stride));
};

float3 LoadVertexAttribute3(ByteAddressBuffer buffer, uint offset, uint stride)
{
    if (stride == 0xFFFFFFFF)
        return float3(0, 0, 0);
    return asfloat(buffer.Load3(offset + stride));
};

ConstantBuffer<interop::ForwardRP> CB : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
PS_INPUT main(uint vertexID : SV_VertexID)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[CB.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[CB.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[CB.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[CB.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[meshData.indexBuffer];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBuffer];
    
    // Fetch vertex data
    uint baseIndex = indexBuffer[vertexID + meshData.indexOffset];
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexPositionStride);
    float3 normal = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexNormalStride);
    float2 uv0 = LoadVertexAttribute2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Stride);
        
    // Transform
    float4 posWorld = mul(float4(pos, 1.0f), instanceData.modelMatrix);
    float4 posClip = mul(posWorld, sceneData.viewProjectionMatrix);
    const float3x3 normalMatrix = (float3x3) transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normal, normalMatrix));
        
    // Output
    PS_INPUT output;
    output.pos = posClip;
    output.normal = normalWorld;
    output.uv = uv0;
    return output;
}
