#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Material.hlsli"

ConstantBuffer<interop::MultiInstanceDrawConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 GBuffer0 : SV_Target0;
    float4 GBuffer1 : SV_Target1;
    float4 GBuffer2 : SV_Target2;
    float4 GBuffer3 : SV_Target3;
};

[RootSignature(BindlessRootSignature)]
PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];    
    
    interop::MaterialData matData = materialsBuffer[Constants.materialIndex];

    PS_OUTPUT output;
    
    MaterialTextureSample texturesSample = SampleMaterialTextures(input.uv, matData);
    MaterialSample matSample = EvaluateSceneMaterial(input.normal, input.tangent, matData, texturesSample, isFrontFace);
    
    // GBuffer0: diffuseAlbedo.rgb + opacity.w
    output.GBuffer0 = float4(matSample.diffuseAlbedo, matSample.opacity);

    // GBuffer1: specularF0.rgb + occlusion.w
    output.GBuffer1 = float4(matSample.specularF0, matSample.occlusion);
    
    // GBuffer2: Normal.xy + roughnes.z + metalness.w
    // metalness is actually not needed since we have speculatF0, but keeping here so we can show metalness in MaterialChannels
    output.GBuffer2 = float4(EncodeNormal(matSample.normal), matSample.roughness, matSample.metalness);
    
    // GBuffer3: Emissive.rgb
    output.GBuffer3 = float4(matSample.emissiveColor, 0.0);
        
    return output;
}
