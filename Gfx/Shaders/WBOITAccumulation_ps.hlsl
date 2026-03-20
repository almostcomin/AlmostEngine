#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Material.hlsli"
#include "Shading.hlsli"
#include "Shadowmap.hlsli"

ConstantBuffer<interop::WBOITAccumStageConstants> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
    float3 posView : TEXCOORD1;
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
    
    StructuredBuffer<interop::DirLightData> dirLightsDataBuffer = ResourceDescriptorHeap[sceneData.dirLightsDataDI];
    StructuredBuffer<interop::PointLightData> pointLightsDataBuffer = ResourceDescriptorHeap[sceneData.pointLightsDataDI];
    StructuredBuffer<interop::SpotLightData> spotLightsDataBuffer = ResourceDescriptorHeap[sceneData.spotLightsDataDI];    
    
    // Get surface material
    interop::MaterialData matData = materialsBuffer[DrawConstants.materialIndex];
    MaterialTextureSample texturesSample = SampleMaterialTextures(input.uv, matData);
    MaterialSample surfaceMat = EvaluateSceneMaterial(input.normal, input.tangent, matData, texturesSample, isFrontFace);
    
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
    float3 diffuseRadiance;
    float3 specularRadiance;
    
    // Main directional light, the only one with a cascade shadowmap
    ShadeSurface_DirLight(sceneData.mainDirLight, surfaceMat, surfacePosView.xyz, viewIncident, diffuseRadiance, specularRadiance);
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
    
    // Ambient    
    float3 ambientColor = lerp(sceneData.ambientBottom.rgb, sceneData.ambientTop.rgb, surfaceMat.normal.y * 0.5 + 0.5);
    diffuseRadiance += ambientColor * surfaceMat.diffuseAlbedo * ambientOcclusion;
        
    float specAO = GetSpecularAO(surfaceMat.normal, surfaceMat.roughness, viewIncident, ambientOcclusion);
    specularRadiance += ambientColor * surfaceMat.specularF0 * specAO;

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