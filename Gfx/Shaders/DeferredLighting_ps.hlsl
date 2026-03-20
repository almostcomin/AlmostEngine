#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Shading.hlsli"
#include "Shadowmap.hlsli"

// Keep in sync with st::gfx::DeferredLightingRenderStage::MaterialChannel
static const uint MaterialChannel_Disabled      = 0;
static const uint MaterialChannel_BaseColor     = 1;
static const uint MaterialChannel_Metalness     = 2;
static const uint MaterialChannel_Anisotropy    = 3;
static const uint MaterialChannel_Roughness     = 4;
static const uint MaterialChannel_Scattering    = 5;
static const uint MaterialChannel_Translucency  = 6;
static const uint MaterialChannel_NormalMap     = 7;
static const uint MaterialChannel_OcclusionMap  = 8;
static const uint MaterialChannel_Emissive      = 9;
static const uint MaterialChannel_SpecularF0    = 10;

ConstantBuffer<interop::DeferredLightingConstants> Constants : register(b0);

MaterialSample DecodeGBuffer(float4 channels[4])
{
    MaterialSample surface = DefaultMaterialSample();
    
    surface.diffuseAlbedo = channels[0].xyz;
    surface.opacity = channels[0].w;
    surface.specularF0 = channels[1].xyz;
    surface.occlusion = channels[1].w;
    surface.normal = DecodeNormal(channels[2].xy);
    surface.roughness = channels[2].z;
    surface.metalness = channels[2].w;
    surface.emissiveColor = channels[3].xyz;
    
    return surface;
}

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
        
    MaterialSample surfaceMat = DecodeGBuffer(gbuffers);

    float3 color = 0.0;
    if (Constants.MaterialChannel == MaterialChannel_BaseColor)
    {
        color = surfaceMat.diffuseAlbedo;
    }
    else if (Constants.MaterialChannel == MaterialChannel_Metalness)
    {
        color = surfaceMat.metalness;
    }
    else if (Constants.MaterialChannel == MaterialChannel_Anisotropy)
    {
        color = 1.f; // TODO
    }
    else if (Constants.MaterialChannel == MaterialChannel_Roughness)
    {
        color = surfaceMat.roughness;
    }
    else if (Constants.MaterialChannel == MaterialChannel_Scattering)
    {
        color = 1.f; // TODO
    }
    else if (Constants.MaterialChannel == MaterialChannel_Translucency)
    {
        color = surfaceMat.opacity;
    }
    else if (Constants.MaterialChannel == MaterialChannel_NormalMap)
    {
        color = mul((float3x3)sceneData.invCamViewMatrix, surfaceMat.normal) * 0.5 + 0.5;
    }
    else if (Constants.MaterialChannel == MaterialChannel_OcclusionMap)
    {
        color = surfaceMat.occlusion;
    }
    else if (Constants.MaterialChannel == MaterialChannel_Emissive)
    {
        color = surfaceMat.emissiveColor;
    }
    else if (Constants.MaterialChannel == MaterialChannel_SpecularF0)
    {
        color = surfaceMat.specularF0;
    }
    else if (Constants.ShowSSAO != 0)
    {
        color = 1.0;
        if (Constants.SSAO_DI != INVALID_DESCRIPTOR_INDEX)
        {
            Texture2D<float4> SSAOTex = ResourceDescriptorHeap[Constants.SSAO_DI];
            color = abs(SSAOTex.SampleLevel(pointClampSampler, input.uv, 0).r);
        }
    }
    else if (Constants.ShowShadowmap != 0)
    {
        color = 1.0;
        float depth = sceneDepth.SampleLevel(pointClampSampler, input.uv, 0).r;
        float4 surfacePosView = PosReconstruction(input.uv, depth, sceneData.invCamProjMatrix);
        
        if (Constants.shadowMapDI != INVALID_DESCRIPTOR_INDEX)
        {
            Texture2D shadowMap = ResourceDescriptorHeap[Constants.shadowMapDI];
            color = SampleShadowMapPoissonDisk16(
                surfacePosView, sceneData.shadowMapViewToClipMatrix, shadowMap, Constants.oneOverShadowmapResolution, 2.0);
        }
    }
    else
    {
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

        color = diffuseRadiance + specularRadiance + surfaceMat.emissiveColor;
    }
    
    return float4(color, 1.0);
}