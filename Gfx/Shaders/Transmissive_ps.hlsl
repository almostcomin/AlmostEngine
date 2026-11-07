// ALM_REQUIRE_VARIANT(ALPHA_TEST)

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "SceneLights.hlsli"

ConstantBuffer<interop::TransmissiveStageConstants> StageConstants : register(b0);

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
    float4 color : SV_Target0;
    float4 blendFactor : SV_Target1;
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
    
#if ALPHA_TEST
    clip(surfaceMat.opacity - matData.alphaCutoff);
#endif
    
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

    float3 viewIncident = normalize(surfacePosView.xyz);
    float NdotV = saturate(-dot(surfaceMat.normal, viewIncident));
    float fresnel = 0.04 + 0.96 * pow(1.0 - NdotV, 5.0);
    
    PS_OUTPUT output;
    
    output.color = float4(diffuseRadiance * (1.0 - surfaceMat.transmission)
                        + specularRadiance + surfaceMat.emissiveColor, 1.0);
    output.blendFactor = float4(surfaceMat.transmission * (1.0 - fresnel)
                        * surfaceMat.baseColor * (1.0 - surfaceMat.metalness), 1.0);
            
    return output;
}
