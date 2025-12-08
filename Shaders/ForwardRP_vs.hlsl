#include "Interop/ConstantBuffers.h"
#include "Interop/ForwardRP.h"
#include "Interop/ConstantBuffers.h"
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
    ConstantBuffer<interop::CameraCB> cameraData = ResourceDescriptorHeap[CB.CameraCBIndex];
    ConstantBuffer<interop::MeshCB> meshData = ResourceDescriptorHeap[CB.MeshCBIndex];
    ConstantBuffer<interop::TransformCB> transformData = ResourceDescriptorHeap[CB.TransformCBIndex];
        
    StructuredBuffer<uint16_t> indexBuffer = ResourceDescriptorHeap[meshData.indexBuffer];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBuffer];
    
    // Fetch vertex data
    uint baseIndex = indexBuffer[vertexID + meshData.indexOffset];
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    float3 pos = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexPosStride);
    float3 normal = LoadVertexAttribute3(vertexBuffer, vertexBufferOffset, meshData.vertexNormalStride);
    float2 uv0 = LoadVertexAttribute2(vertexBuffer, vertexBufferOffset, meshData.vertexTexCoord0Stride);
        
    // Transform
    float4 posWorld = mul(float4(pos, 1.0f), transformData.modelMatrix);
    float4 posClip = mul(posWorld, cameraData.viewProjectionMatrix);    
    const float3x3 normalMatrix = (float3x3) transpose(transformData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normal, normalMatrix));
        
    // Output
    PS_INPUT output;
    output.pos = posClip;
    output.normal = normalWorld;
    output.uv = uv0;
    return output;
}
