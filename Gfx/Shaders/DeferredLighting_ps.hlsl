#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
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
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    Texture2D sceneDepth = ResourceDescriptorHeap[Constants.sceneDepthDI];
    Texture2D GBuffer0 = ResourceDescriptorHeap[Constants.GBuffer0DI]; // SurfaceAlbedo.rgb + MaterialID.z
    Texture2D GBuffer1 = ResourceDescriptorHeap[Constants.GBuffer1DI]; // Normal.xy + Roughness.z
    Texture2D GBuffer2 = ResourceDescriptorHeap[Constants.GBuffer2DI]; // Metallic.x + AO.z
    Texture2D GBuffer3 = ResourceDescriptorHeap[Constants.GBuffer3DI]; // Emissive.rgb
                    
    // Sample G-Buffers
    float4 g0 = GBuffer0.Sample(pointClampSampler, input.uv);
    float4 g1 = GBuffer1.Sample(pointClampSampler, input.uv);
    float4 g2 = GBuffer2.Sample(pointClampSampler, input.uv);
    float4 g3 = GBuffer3.Sample(pointClampSampler, input.uv);
    
    float3 surfaceAlbedo = g0.rgb;
    float3 surfaceNormal = DecodeNormal(g1.xy);
    
    float metallic = g2.x;
    float roughness = g1.z;
    float3 surfaceEmissive = g3.rgb;
        
    // World pos reconstruction
    float4 worldPos = WorldPosReconstruction(input.uv, sceneDepth, sceneData.invCamViewProjMatrix);
    // Retrieve shadow factor
    float shadowFactor = 1.0;
    if (Constants.shadowMapDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D shadowMap = ResourceDescriptorHeap[Constants.shadowMapDI];
        shadowFactor = SampleShadowMap(worldPos, sceneData.sunWorldToClipMatrix, shadowMap);
    }
    
    // Diffuse
    float NdotL = saturate(dot(surfaceNormal, -sceneData.sunDirection));
    float3 diffuseTerm = surfaceAlbedo * sceneData.sunColor.xyz * sceneData.sunIntensity * shadowFactor * NdotL;
    
    // Ambient    
    float3 ambientColor = lerp(sceneData.ambientBottom.rgb, sceneData.ambientTop.rgb, surfaceNormal.y * 0.5 + 0.5);
    diffuseTerm += ambientColor * surfaceAlbedo; // TODO: Ambient occlusion
        
    float3 color = diffuseTerm + surfaceEmissive;
    return float4(color, 1.0);
}