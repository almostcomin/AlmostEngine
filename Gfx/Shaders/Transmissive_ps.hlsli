#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Material.hlsli"
#include "Shading.hlsli"
#include "Shadowmap.hlsli"

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
    
    StructuredBuffer<interop::DirLightData> dirLightsDataBuffer = ResourceDescriptorHeap[sceneData.dirLightsDataDI];
    StructuredBuffer<interop::PointLightData> pointLightsDataBuffer = ResourceDescriptorHeap[sceneData.pointLightsDataDI];
    StructuredBuffer<interop::SpotLightData> spotLightsDataBuffer = ResourceDescriptorHeap[sceneData.spotLightsDataDI];    
    
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
    float shadowFactor = 1.0;
    if (StageConstants.shadowMapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D shadowMap = ResourceDescriptorHeap[StageConstants.shadowMapDI];
        shadowFactor = SampleShadowMapPoissonDisk16(
                surfacePosView, sceneData.shadowMapViewToClipMatrix, shadowMap, StageConstants.oneOverShadowmapResolution, 2.0);
    }

    // Sample ambient occlusion
    float ambientOcclusion = surfaceMat.occlusion;
    if (StageConstants.SSAO_DI != INVALID_DESCRIPTOR_INDEX)
    {
        float2 screenUV = input.pos.xy * sceneData.invScreenResolution;
        Texture2D<float> SSAOTex = ResourceDescriptorHeap[StageConstants.SSAO_DI];
        ambientOcclusion *= SSAOTex.SampleLevel(pointClampSampler, screenUV, 0);
    }

    // Shade
    float3 viewIncident = normalize(surfacePosView.xyz);
    float3 diffuseRadiance = float3(0.0, 0.0, 0.0);
    float3 specularRadiance = float3(0.0, 0.0, 0.0);
    
    // Main directional light, the only one with a cascade shadowmap
    if (sceneData.mainDirLightEnabled != 0)
    {
        ShadeSurface_DirLight(sceneData.mainDirLight, surfaceMat, surfacePosView.xyz, viewIncident, diffuseRadiance, specularRadiance);
    }
    // Cascade shadow map only affects main directional light
    diffuseRadiance *= shadowFactor;
    specularRadiance *= shadowFactor;
    
    // Additional directional lights
    for (uint i = 0; i < sceneData.dirLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;
        interop::DirLightData dirLight = dirLightsDataBuffer[i];
            
        ShadeSurface_DirLight(dirLight, surfaceMat, surfacePosView.xyz, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        diffuseRadiance += lightDiffuseRadiance;
        specularRadiance += lightSpecularRadiance;
    }

    // Point lights
    for (uint i = 0; i < sceneData.pointLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;            
        interop::PointLightData pointLight = pointLightsDataBuffer[i];
            
        ShadeSurface_PointLight(pointLight, surfaceMat, surfacePosView.xyz, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        diffuseRadiance += lightDiffuseRadiance;
        specularRadiance += lightSpecularRadiance;
    }

    // Spot lights
    for (uint i = 0; i < sceneData.spotLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;
        interop::SpotLightData spotLight = spotLightsDataBuffer[i];
            
        ShadeSurface_SpotLight(spotLight, surfaceMat, surfacePosView.xyz, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        diffuseRadiance += lightDiffuseRadiance;
        specularRadiance += lightSpecularRadiance;
    }
    
    // IBL / Ambient
    float specAO = GetSpecularAO(surfaceMat.normal, surfaceMat.roughness, viewIncident, ambientOcclusion);
    if (sceneData.IBLEnabled)
    {
        float3 iblDiffuse;
        float3 iblSpecular;
        TextureCube lambCube = ResourceDescriptorHeap[sceneData.IBLLambCubemapTextureDI];
        TextureCube ggxCube = ResourceDescriptorHeap[sceneData.IBLGGXCubemapTextureDI];
                
        ShaderSurface_IBL(lambCube, ggxCube, sceneData.IBLGGXCubeMipCount, (float3x3) sceneData.invCamViewMatrix, surfaceMat, viewIncident, iblDiffuse, iblSpecular);
        
        diffuseRadiance += iblDiffuse * ambientOcclusion;
        specularRadiance += iblSpecular * specAO;
    }
    else
    {
        float3 ambientColor = lerp(sceneData.ambientBottom.rgb, sceneData.ambientTop.rgb, surfaceMat.normal.y * 0.5 + 0.5);
        diffuseRadiance += ambientColor * surfaceMat.diffuseAlbedo * ambientOcclusion;
        specularRadiance += ambientColor * surfaceMat.specularF0 * specAO;
    }
    
    float NdotV = saturate(-dot(surfaceMat.normal, viewIncident));
    float fresnel = 0.04 + 0.96 * pow(1.0 - NdotV, 5.0);
    
    PS_OUTPUT output;
    
    output.color = float4(diffuseRadiance * (1.0 - surfaceMat.transmission)
                        + specularRadiance + surfaceMat.emissiveColor, 1.0);
    output.blendFactor = float4(surfaceMat.transmission * (1.0 - fresnel)
                        * surfaceMat.baseColor * (1.0 - surfaceMat.metalness), 1.0);
            
    return output;
}
