#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::OpaqueStage> Constants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[Constants.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[Constants.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];

    float4 out_col = float4(1.0, 1.0, 1.0, 1.0);
    if (meshData.textureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D albedo = ResourceDescriptorHeap[meshData.textureDI];    
        out_col = albedo.Sample(linearWrapSampler, input.uv);
    }
    return out_col;
}
