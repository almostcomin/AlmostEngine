#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Shading.hlsli"

MaterialSample DecodeGBuffer(float4 channels[4])
{
    MaterialSample surface = DefaultMaterialSample();
    
    surface.diffuseAlbedo = channels[0].xyz;
    surface.opacity = channels[0].w;
    surface.specularF0 = channels[1].xyz;
    surface.occlusion = channels[1].w;
    surface.normal = DecodeNormal(channels[2].xy);
    surface.roughness = channels[2].z;
    surface.emissiveColor = channels[3].xyz;
    
    return surface;
}

ConstantBuffer<interop::DeferredLightingConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    Texture2D sceneDepth = ResourceDescriptorHeap[Constants.sceneDepthDI];
    Texture2D GBuffer0 = ResourceDescriptorHeap[Constants.GBuffer0DI];
    Texture2D GBuffer1 = ResourceDescriptorHeap[Constants.GBuffer1DI];
    Texture2D GBuffer2 = ResourceDescriptorHeap[Constants.GBuffer2DI];
    Texture2D GBuffer3 = ResourceDescriptorHeap[Constants.GBuffer3DI];
                    
    // Sample G-Buffers
    float4 gbuffers[4];
    gbuffers[0] = GBuffer0.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[1] = GBuffer1.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[2] = GBuffer2.SampleLevel(pointClampSampler, input.uv, 0);
    gbuffers[3] = GBuffer3.SampleLevel(pointClampSampler, input.uv, 0);
        
    MaterialSample surfaceMat = DecodeGBuffer(gbuffers);
            
    // World pos reconstruction
    float depth = sceneDepth.SampleLevel(pointClampSampler, input.uv, 0).r;
    float4 surfacePos = PosReconstruction(input.uv, depth, sceneData.invCamViewProjMatrix);
    
    // Retrieve shadow factor
    float shadowFactor = 1.0;
    if (Constants.shadowMapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D shadowMap = ResourceDescriptorHeap[Constants.shadowMapDI];
        shadowFactor = SampleShadowMap(surfacePos, sceneData.sunWorldToClipMatrix, shadowMap);
    }
    //shadowFactor *= surfaceMat.occlusion;
    
    if (Constants.SSAO_DI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D<float> SSAOTex = ResourceDescriptorHeap[Constants.SSAO_DI];
        shadowFactor *= SSAOTex.SampleLevel(pointClampSampler, input.uv, 0);
    }
    
    LightConstants sunConstants;
    sunConstants.lighType = LightType_Directional;
    sunConstants.direction = sceneData.sunDirection;
    sunConstants.intensity = sceneData.sunIrradiance;
    sunConstants.angularSizeOrInvRange = sceneData.sunAngularSizeRad;
    
    // Shade
    float3 viewIncident = normalize(surfacePos.xyz - sceneData.camWorldPos.xyz);    
    float3 diffuseRadiance = 0.0;
    float3 specularRadiance = 0.0;
    ShadeSurface(sunConstants, surfaceMat, surfacePos.xyz, viewIncident, diffuseRadiance, specularRadiance);
    
    float3 diffuseTerm = shadowFactor * diffuseRadiance * sceneData.sunColor.rgb;
    //diffuseTerm = surfaceMat.occlusion;
    float3 specularTerm = shadowFactor * specularRadiance * sceneData.sunColor.rgb;
            
    // Ambient    
    float3 ambientColor = lerp(sceneData.ambientBottom.rgb, sceneData.ambientTop.rgb, surfaceMat.normal.y * 0.5 + 0.5);
    diffuseTerm += ambientColor * surfaceMat.diffuseAlbedo;
    specularTerm += ambientColor * surfaceMat.specularF0;

    float3 color = diffuseTerm + specularTerm + surfaceMat.emissiveColor;
        
    return float4(color, 1.0);
}