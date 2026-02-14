#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SSAOConstants> Constants : register(b0);

// Kernel samples (hemisphere)
static const float3 samples[16] =
{
    float3(0.053, 0.053, 0.132), float3(-0.113, 0.071, 0.212),
    float3(0.138, -0.092, 0.184), float3(-0.074, -0.133, 0.267),
    float3(0.179, 0.143, 0.357), float3(-0.238, 0.132, 0.397),
    float3(0.153, -0.204, 0.409), float3(-0.186, -0.239, 0.465),
    float3(0.283, 0.226, 0.453), float3(-0.370, 0.185, 0.556),
    float3(0.265, -0.337, 0.627), float3(-0.314, -0.386, 0.605),
    float3(0.468, 0.410, 0.702), float3(-0.563, 0.313, 0.813),
    float3(0.482, -0.533, 0.786), float3(-0.577, -0.601, 0.841)
};

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[Constants.depthTextureDI];
    Texture2D<float4> normalsTexture = ResourceDescriptorHeap[Constants.normalsTextureDI];
    Texture2D<float2> noiseTexture = ResourceDescriptorHeap[Constants.noiseTextureDI];
    RWTexture2D<float> outputAOTexture = ResourceDescriptorHeap[Constants.outputAOTextureDI];
        
    uint2 outputSize = uint2(Constants.textureWidth, Constants.textureHeight);    
    if (any(DTid >= outputSize))
        return;
    
    float2 uv = (float2(DTid) + 0.5) / float2(outputSize);
        
    // Get depth
    float linearDepth = depthTexture.SampleLevel(pointClampSampler, uv, 0).r;
    if (linearDepth > 10000.0f)
    {
        outputAOTexture[DTid] = 1.0;
        return;
    }
        
    // Get normal
    float3 normalWorld = DecodeNormal(normalsTexture.SampleLevel(pointClampSampler, uv, 0).xy);
    float3 normal = mul((float3x3) sceneData.camViewMatrix, normalWorld);
    normal = normalize(normal);

    // View space position
    float3 viewRay = Constants.frustumTopLeft.xyz + (uv.x * Constants.frustumVecX.xyz) + (uv.y * Constants.frustumVecY.xyz);
    // viewRay.z is -1.0, so mul by linearDepth to get pos
    float3 viewPos = viewRay * linearDepth;
            
    // Random rotation
    float2 noiseVal = noiseTexture.SampleLevel(linearWrapSampler, uv * Constants.noiseScale, 0);
    float3 randomVec = float3(noiseVal, 0.0);
    // TBN matrix
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    
    [unroll]
    for (uint i = 0; i < 16; i++)
    {
        // Sample in view space
        float3 samplePos = viewPos + mul(samples[i], TBN) * Constants.radius;
        
        // Project to screen space
        float4 offset = mul(sceneData.camProjMatrix, float4(samplePos, 1.0));
        offset.xyz /= offset.w;
        float2 sampleUV = offset.xy * float2(0.5, -0.5) + 0.5;
        
        // Sample depth
        float sampleDepth = depthTexture.SampleLevel(pointClampSampler, sampleUV, 0);
        float sampleDistToCam = -samplePos.z;
        
        // Range check. Avoid far object acclude
        float depthDiff = abs(linearDepth - sampleDistToCam);
        float rangeCheck = exp(-depthDiff * depthDiff / (Constants.radius * 0.5));
                
        // Check comapre sample depth (.z) with its depth in the depthTexture
        if (sampleDepth < sampleDistToCam - Constants.bias)
        {
            occlusion += rangeCheck;
        }
    }
    
    occlusion = 1.0 - (occlusion / 16.0);
    occlusion = pow(occlusion, Constants.power);
    
    outputAOTexture[DTid] = occlusion;
}