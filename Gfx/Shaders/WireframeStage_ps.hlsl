#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::SingleInstanceDrawData> Constants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
#if 0
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
    interop::MaterialData matData = materialsBuffer[meshData.materialIdx];

    float4 out_col = matData.baseColor;
    if (matData.baseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[matData.baseColorTextureDI];
        float4 texColor = baseColorTexture.Sample(linearWrapSampler, input.uv);
        out_col.rgb = texColor.rgb;
        out_col.a *= texColor.a;
    }
#else
    float4 out_col = 1.0f;
#endif
    return out_col;
}
