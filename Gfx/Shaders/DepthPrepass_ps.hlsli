#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Material.hlsli"

ConstantBuffer<interop:: MultiInstanceDrawConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
#if ALPHA_TEST
    float2 uv : TEXCOORD0;
#endif
};

[RootSignature(BindlessRootSignature)]
void main(PS_INPUT input)
{
#if ALPHA_TEST
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];    
    interop::MaterialData matData = materialsBuffer[Constants.materialIndex];

    float alpha = SampleBaseColorAlpha(input.uv, matData);
    clip(alpha - matData.alphaCutoff);
#endif
}