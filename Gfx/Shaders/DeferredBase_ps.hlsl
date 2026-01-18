#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SingleInstanceDrawData> Constants : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT; // xyz = tangent, w = handedness (-1 or +1)
    float2 uv : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 GBuffer0 : SV_Target0;
    float4 GBuffer1 : SV_Target1;
    float4 GBuffer2 : SV_Target2;
    float4 GBuffer3 : SV_Target3;
};

[RootSignature(BindlessRootSignature)]
PS_OUTPUT main(PS_INPUT input)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    StructuredBuffer<interop::MaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.materialsBufferDI];    
    
    interop::InstanceData instanceData = instancesBuffer[Constants.instanceIdx];
    interop::MeshData meshData = meshesBuffer[instanceData.meshIndex];
    interop::MaterialData matData = materialsBuffer[meshData.materialIdx];

    PS_OUTPUT output;
    
    // GBuffer0: BaseColor + MaterialID
    float4 baseColor = matData.baseColor;
    if (matData.baseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[matData.baseColorTextureDI];
        float4 texColor = baseColorTexture.Sample(linearWrapSampler, input.uv);
        baseColor.rgb = texColor.rgb;
        baseColor.a *= texColor.a;
    }
    output.GBuffer0 = float4(baseColor.rgb, 0.0); // materialID = 0
    
    // GBuffer1: Normal.xy + Roughness
    float3 surfaceNormal = normalize(input.normal);

    if (matData.normalTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D normalTexture = ResourceDescriptorHeap[matData.normalTextureDI];
        // Sample normal and remap to [-1,1]
        float3 tangentNormal = normalTexture.Sample(linearWrapSampler, input.uv).xyz;
        tangentNormal = tangentNormal * 2.0 - 1.0;
        // Apply scale
        tangentNormal.xy *= matData.normalScale;
        // Build TBN matrix
        float3 N = surfaceNormal;
        float3 T = normalize(input.tangent.xyz);
        float3 B = cross(N, T) * input.tangent.w; // w is headedness
        float3x3 TBN = float3x3(T, B, N);
        
        surfaceNormal = normalize(mul(tangentNormal, TBN));
    }

    output.GBuffer1 = float4(EncodeNormal(surfaceNormal), 0.5, 0.0); // // roughness = 0.5
    
    // GBuffer2: Metallic + AO
    output.GBuffer2 = float4(0.0, 1.f, 0.0, 0.0); // metallic = 0, AO = 1
    
    // GBuffer3: Emissive
    output.GBuffer3 = float4(0.0, 0.0, 0.0, 0.0);
        
    return output;
}
