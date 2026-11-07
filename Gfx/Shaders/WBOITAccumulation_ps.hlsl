#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "SceneLights.hlsli"

ConstantBuffer<interop::WBOITAccumStageConstants> StageConstants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
    float3 posView : TEXCOORD1;
    nointerpolation uint materialIndex : MATERIAL;
};

struct PS_OUTPUT
{
    float4 AccumRT : SV_Target0;
    float RevealageRT : SV_Target1;
};

[RootSignature(BindlessRootSignature)]
PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];
        
    // Get surface material
    interop::MaterialData matData = materialsBuffer[input.materialIndex];
    MaterialTextureSample texturesSample = SampleMaterialTextures(input.uv, matData);
    MaterialSample surfaceMat = EvaluateSceneMaterial(input.normal, input.tangent, matData, texturesSample, isFrontFace);
    
    // World pos reconstruction
    float4 surfacePosView = float4(input.posView, 1.0);
    
    // Sample cascade shadowmap
    float shadowFactor = SampleMainDirLightShadowmap(
        StageConstants.shadowMapDI, StageConstants.oneOverShadowmapResolution, surfacePosView, sceneData);
    
    // Clouds shadow
    float cloudsShadowFactor = SampleCloudsShadow(StageConstants.CloudsShadowmapDI, surfacePosView.xyz, sceneData);
    
    // Sample ambient occlusion
    float2 screenUV = input.pos.xy * sceneData.invScreenResolution;
    float ambientOcclusion = SampleAmbientOcclusion(StageConstants.SSAO_DI, screenUV, surfaceMat.occlusion);
    
    // Shade
    float3 diffuseRadiance = float3(0.0, 0.0, 0.0);
    float3 specularRadiance = float3(0.0, 0.0, 0.0);
    ShadeSurfaceSceneLights(
        sceneData,
        surfaceMat,
        surfacePosView.xyz,
        shadowFactor * cloudsShadowFactor,
        ambientOcclusion,
        diffuseRadiance,
        specularRadiance);

    float4 color = float4(diffuseRadiance + specularRadiance + surfaceMat.emissiveColor, surfaceMat.opacity);
    
    // Calc output value
    float z = abs(surfacePosView.z);
    float weight =
        max(min(1.0, max(max(color.r, color.g), color.b) * color.a), color.a) *
        clamp(0.03 / (1e-5 + pow(z / 200, 4.0)), 1e-2, 3e3);
    
    PS_OUTPUT output;
    output.AccumRT = float4(color.rgb * color.a, color.a) * weight;
    output.RevealageRT = color.a;
    
    return output;
}