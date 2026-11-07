#ifndef __SCENE_LIGHTS_HLSLI__
#define __SCENE_LIGHTS_HLSLI__

#include "Shading.hlsli"
#include "Shadowmap.hlsli"

float SampleMainDirLightShadowmap(uint shadowMapDI, float2 oneOverShadowmapResolution,
    float4 surfacePosView, interop::SceneConstants sceneData)
{
    float shadowFactor = 1.0;
    if (shadowMapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D shadowMap = ResourceDescriptorHeap[shadowMapDI];
        shadowFactor = SampleShadowMapPoissonDisk16(
            surfacePosView, sceneData.shadowMapViewToClipMatrix, shadowMap, oneOverShadowmapResolution, 2.0);
    }
    
    return shadowFactor;
}

float SampleCloudsShadow(uint cloudsShadowmapDI, float3 surfacePosView, interop::SceneConstants sceneData)
{
    float cloudsShadowFactor = 1.0;
    if (cloudsShadowmapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D cloudsShadowmap = ResourceDescriptorHeap[cloudsShadowmapDI];
            
        // Convert view-space surface position to world space
        float4 surfacePosWorldH = mul(sceneData.invCamViewMatrix, float4(surfacePosView, 1));
        float3 surfacePosWorld = surfacePosWorldH.xyz;
            
        // Compute clip-space UV from sun-space direction
        float4 clip = mul(sceneData.CloudsShadowmapWorldToClipMatrix, float4(surfacePosWorld, 1.0));
        float2 uv = float2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
        cloudsShadowFactor = cloudsShadowmap.SampleLevel(linearClampSampler, uv, 0.0).x;
    }
    return cloudsShadowFactor;
}

float SampleAmbientOcclusion(uint ssaoDI, float2 ssaoUV, float baseAO)
{
    float ambientOcclusion = baseAO;
    if (ssaoDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D<float> SSAOTex = ResourceDescriptorHeap[ssaoDI];
        ambientOcclusion *= SSAOTex.SampleLevel(pointClampSampler, ssaoUV, 0);
    }
    return ambientOcclusion;           
}

void ShadeSurfaceSceneLights(
    interop::SceneConstants sceneData,
    MaterialSample surfaceMat,
    float3 surfacePosView,
    float mainDirLightShadowFactor,
    float ambientOcclusion,
    out float3 out_diffuseRadiance,
    out float3 out_specularRadiance)
{
    StructuredBuffer<interop::DirLightData> dirLightsDataBuffer = ResourceDescriptorHeap[sceneData.dirLightsDataDI];
    StructuredBuffer<interop::PointLightData> pointLightsDataBuffer = ResourceDescriptorHeap[sceneData.pointLightsDataDI];
    StructuredBuffer<interop::SpotLightData> spotLightsDataBuffer = ResourceDescriptorHeap[sceneData.spotLightsDataDI];    
    float3 viewIncident = normalize(surfacePosView);
    
    out_diffuseRadiance = float3(0.0, 0.0, 0.0);
    out_specularRadiance = float3(0.0, 0.0, 0.0);
    
    // Main directional light, the only one with a cascade shadowmap
    if (sceneData.mainDirLightEnabled != 0)
    {
        ShadeSurface_DirLight(sceneData.mainDirLight, surfaceMat, surfacePosView, viewIncident, out_diffuseRadiance, out_specularRadiance);
    }
    out_diffuseRadiance *= mainDirLightShadowFactor;
    out_specularRadiance *= mainDirLightShadowFactor;

    // Additional directional lights
    for (uint i = 0; i < sceneData.dirLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;
        interop::DirLightData dirLight = dirLightsDataBuffer[i];
            
        ShadeSurface_DirLight(dirLight, surfaceMat, surfacePosView, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        out_diffuseRadiance += lightDiffuseRadiance;
        out_specularRadiance += lightSpecularRadiance;
    }

    // Point lights
    for (uint i = 0; i < sceneData.pointLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;
        interop::PointLightData pointLight = pointLightsDataBuffer[i];
            
        ShadeSurface_PointLight(pointLight, surfaceMat, surfacePosView.xyz, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        out_diffuseRadiance += lightDiffuseRadiance;
        out_specularRadiance += lightSpecularRadiance;
    }

    // Spot lights
    for (uint i = 0; i < sceneData.spotLightCount; i++)
    {
        float3 lightDiffuseRadiance;
        float3 lightSpecularRadiance;
        interop::SpotLightData spotLight = spotLightsDataBuffer[i];
            
        ShadeSurface_SpotLight(spotLight, surfaceMat, surfacePosView.xyz, viewIncident, lightDiffuseRadiance, lightSpecularRadiance);
            
        out_diffuseRadiance += lightDiffuseRadiance;
        out_specularRadiance += lightSpecularRadiance;
    }
        
    float specAO = GetSpecularAO(surfaceMat.normal, surfaceMat.roughness, viewIncident, ambientOcclusion);
    
    // IBL / Ambient
    if (sceneData.IBLEnabled)
    {
        float3 iblDiffuse;
        float3 iblSpecular;
        TextureCube lambCube = ResourceDescriptorHeap[sceneData.IBLLambCubemapTextureDI];
        TextureCube ggxCube = ResourceDescriptorHeap[sceneData.IBLGGXCubemapTextureDI];
                
        ShaderSurface_IBL(lambCube, ggxCube, sceneData.IBLGGXCubeMipCount, (float3x3)sceneData.invCamViewMatrix, surfaceMat, viewIncident,
            iblDiffuse, iblSpecular);
        
        out_diffuseRadiance += iblDiffuse * ambientOcclusion;
        out_specularRadiance += iblSpecular * specAO;
    }
    else
    {
        float3 ambientColor = lerp(sceneData.ambientBottom.rgb, sceneData.ambientTop.rgb, surfaceMat.normal.y * 0.5 + 0.5);
        out_diffuseRadiance += ambientColor * surfaceMat.diffuseAlbedo * ambientOcclusion;
        out_specularRadiance += ambientColor * surfaceMat.specularF0 * specAO;
    }
}

#endif // __SCENE_LIGHTS_HLSLI__