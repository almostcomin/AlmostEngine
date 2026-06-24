#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Material.hlsli"
#include "GBuffersCommon.hlsli"

ConstantBuffer<interop::GBufferStageConstats> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
};

typedef GBufferData PS_OUTPUT;

[RootSignature(BindlessRootSignature)]
PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];    
    
    interop::MaterialData matData = materialsBuffer[DrawConstants.materialIndex];
    
    MaterialTextureSample texturesSample = SampleMaterialTextures(input.uv, matData);
    MaterialSample matSample = EvaluateSceneMaterial(input.normal, input.tangent, matData, texturesSample, isFrontFace);
    
#if ALPHA_TEST
    clip(matSample.opacity - matData.alphaCutoff);
#endif

    PS_OUTPUT output = EncodeGBuffers(matSample);
            
    return output;
}
