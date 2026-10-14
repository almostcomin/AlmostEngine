#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Shading.hlsli"
#include "Shadowmap.hlsli"
#include "GBuffersCommon.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::DeferredLightingConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    Texture2D sceneDepth = ResourceDescriptorHeap[Constants.sceneDepthDI];
    Texture2D GBuffer0 = ResourceDescriptorHeap[Constants.GBuffer0DI];
    Texture2D GBuffer1 = ResourceDescriptorHeap[Constants.GBuffer1DI];
    Texture2D GBuffer2 = ResourceDescriptorHeap[Constants.GBuffer2DI];
    Texture2D GBuffer3 = ResourceDescriptorHeap[Constants.GBuffer3DI];
    
    StructuredBuffer<interop::DirLightData> dirLightsDataBuffer = ResourceDescriptorHeap[sceneData.dirLightsDataDI];
    StructuredBuffer<interop::PointLightData> pointLightsDataBuffer = ResourceDescriptorHeap[sceneData.pointLightsDataDI];
    StructuredBuffer<interop::SpotLightData> spotLightsDataBuffer = ResourceDescriptorHeap[sceneData.spotLightsDataDI];
                    
    // Sample G-Buffers
    float4 gbuffers[4];
    gbuffers[0] = GBuffer0.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[1] = GBuffer1.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[2] = GBuffer2.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[3] = GBuffer3.SampleLevel(pointClampSampler, input.uv, 0);
        
    MaterialSample surfaceMat = DecodeGBuffers(gbuffers);

    // World pos reconstruction
    float depth = sceneDepth.SampleLevel(pointClampSampler, input.uv, 0).r;
    float4 surfacePosView = PosReconstruction(input.uv, depth, sceneData.invCamProjMatrix);
    
    // Sample cascade shadowmap
    float shadowFactor = 1.0;
    if (Constants.shadowMapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D shadowMap = ResourceDescriptorHeap[Constants.shadowMapDI];
        shadowFactor = SampleShadowMapPoissonDisk16(
            surfacePosView, sceneData.shadowMapViewToClipMatrix, shadowMap, Constants.oneOverShadowmapResolution, 2.0);
    }
        
    // Clouds shadow
    float cloudsShadowFactor = 1.0;
    if (Constants.CloudsShadowmapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D cloudsShadowmap = ResourceDescriptorHeap[Constants.CloudsShadowmapDI];
            
        // Convert view-space surface position to world space
        float4 surfacePosWorldH = mul(sceneData.invCamViewMatrix, float4(surfacePosView.xyz, 1));
        float3 surfacePosWorld = surfacePosWorldH.xyz;
            
        // Compute clip-space UV from sun-space direction
        float4 clip = mul(sceneData.CloudsShadowmapWorldToClipMatrix, float4(surfacePosWorld, 1.0));
        float2 uv = float2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
        cloudsShadowFactor = cloudsShadowmap.SampleLevel(linearClampSampler, uv, 0.0).x;
    }
    
    // Sample ambient occlusion
    float ambientOcclusion = surfaceMat.occlusion;
    if (Constants.SSAO_DI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D<float> SSAOTex = ResourceDescriptorHeap[Constants.SSAO_DI];
        ambientOcclusion *= SSAOTex.SampleLevel(pointClampSampler, input.uv, 0);
    }
        
    // Shade
    float3 viewIncident = normalize(surfacePosView.xyz);
    float3 diffuseRadiance;
    float3 specularRadiance;

    // Main directional light, the only one with a cascade shadowmap
    ShadeSurface_DirLight(sceneData.mainDirLight, surfaceMat, surfacePosView.xyz, viewIncident, diffuseRadiance, specularRadiance);
    // Cascade shadow map only affects main directional light
    float combinedShadowFactor = shadowFactor * cloudsShadowFactor;
    diffuseRadiance *= combinedShadowFactor;
    specularRadiance *= combinedShadowFactor;

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

    float3 color = diffuseRadiance + specularRadiance + surfaceMat.emissiveColor;
    color = ApplyExposure(color, sceneData.exposureFactorTextureDI);
    
    return float4(color, 1.0);
}