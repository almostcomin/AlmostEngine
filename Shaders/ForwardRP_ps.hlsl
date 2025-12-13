#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ForwardRP> CB : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[CB.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[CB.meshesBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[CB.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
            
    Texture2D albedo = ResourceDescriptorHeap[meshData.textureIndex];
    
    float4 out_col = albedo.Sample(linearWrapSampler, input.uv);
    return out_col;
}
