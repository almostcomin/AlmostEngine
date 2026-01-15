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
    Texture2D shadowMap = ResourceDescriptorHeap[Constants.shadowMapDI];
    Texture2D GBuffer0 = ResourceDescriptorHeap[Constants.GBuffer0DI]; // BaseColor.rgb + MaterialID.z
    Texture2D GBuffer1 = ResourceDescriptorHeap[Constants.GBuffer1DI]; // Normal.xy + Roughness.z
    Texture2D GBuffer2 = ResourceDescriptorHeap[Constants.GBuffer2DI]; // Metallic.x + AO.z
    Texture2D GBuffer3 = ResourceDescriptorHeap[Constants.GBuffer3DI]; // Emissive.rgb
                    
    // Sample G-Buffers
    float4 g0 = GBuffer0.Sample(pointClampSampler, input.uv);
    float4 g1 = GBuffer1.Sample(pointClampSampler, input.uv);
    float4 g2 = GBuffer2.Sample(pointClampSampler, input.uv);
    float4 g3 = GBuffer3.Sample(pointClampSampler, input.uv);
    
    float3 baseColor = g0.rgb;
    float3 normal = DecodeNormal(g1.xy);
    
    float metallic = g2.x;
    float roughness = g1.z;
    float3 emissive = g3.rgb;
        
    // World pos reconstruction
    float4 worldPos = WorldPosReconstruction(input.uv, sceneDepth, sceneData.invCamViewProjMatrix);
    // Get shadow factor
    float shadow = SampleShadowMap(worldPos, sceneData.sunWorldToClipMatrix, shadowMap);
    // Lambert
    float NdotL = saturate(dot(normal, -sceneData.sunDirection));
    float3 diffuse = baseColor * sceneData.sunColor * sceneData.sunIntensity * shadow * NdotL;
    
    float3 color = diffuse + emissive;
    return float4(color, 1.0);
}