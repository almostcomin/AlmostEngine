#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "SceneLights.hlsli"
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
    
    // Early exit on infinite-depth
    float depth = sceneDepth.SampleLevel(pointClampSampler, input.uv, 0).r;
    if (depth == 0.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
                    
    // Sample G-Buffers
    float4 gbuffers[4];
    gbuffers[0] = GBuffer0.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[1] = GBuffer1.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[2] = GBuffer2.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[3] = GBuffer3.SampleLevel(pointClampSampler, input.uv, 0);
        
    MaterialSample surfaceMat = DecodeGBuffers(gbuffers);

    // World pos reconstruction
    float4 surfacePosView = PosReconstruction(input.uv, depth, sceneData.invCamProjMatrix);
    
    // Sample cascade shadowmap
    float shadowFactor = SampleMainDirLightShadowmap(
        Constants.shadowMapDI, Constants.oneOverShadowmapResolution, surfacePosView, sceneData);
        
    // Clouds shadow
    float cloudsShadowFactor = SampleCloudsShadow(Constants.CloudsShadowmapDI, surfacePosView.xyz, sceneData);
    
    // Sample ambient occlusion
    float ambientOcclusion = SampleAmbientOcclusion(Constants.SSAO_DI, input.uv, surfaceMat.occlusion);
                
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
    
    float3 color = diffuseRadiance + specularRadiance + surfaceMat.emissiveColor;
    color = ApplyExposure(color, sceneData.exposureFactorTextureDI);
    
    return float4(color, 1.0);
}