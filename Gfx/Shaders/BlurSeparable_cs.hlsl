#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

#define GROUP_SIZE 128
#define RADIUS 4

ConstantBuffer<interop::BlurSeparable> Constants : register(b0);

// For radius 4, sigma 2.0
static const float Weights[9] =
{
    0.026995f, 0.064759f, 0.120985f, 0.176033f, 0.199471f,
    0.176033f, 0.120985f, 0.064759f, 0.026995f
};

// "apron" is the extra space at the edges to edge threads can read neighbour data.
groupshared float cacheAO[GROUP_SIZE + 2 * RADIUS];
groupshared float cacheDepth[GROUP_SIZE + 2 * RADIUS];

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    Texture2D<float> inputAOTex = ResourceDescriptorHeap[Constants.inputAOTextureDI];
    Texture2D<float> depthTex = ResourceDescriptorHeap[Constants.depthTextureDI];
    RWTexture2D<float> outputAOTex = ResourceDescriptorHeap[Constants.outputAOTextureDI];
    
    int globalX = min(DTid.x, Constants.textureDim - 1);
    int localX = GTid.x + RADIUS;
    
    // Fill cache
    cacheAO[localX] = inputAOTex[uint2(globalX, DTid.y)];
    cacheDepth[localX] = depthTex[uint2(globalX, DTid.y)];
    
    if (GTid.x < RADIUS)
    {
        // left apron
        int leftX = max(0, globalX - RADIUS);
        cacheAO[GTid.x] = inputAOTex[uint2(leftX, DTid.y)];
        cacheDepth[GTid.x] = depthTex[uint2(leftX, DTid.y)];
        // right apron
        int rightX = min(Constants.textureWidth - 1, globalX + GROUP_SIZE);
        cacheAO[localX + GROUP_SIZE] = inputAOTex[uint2(rightX, DTid.y)];
        cacheDepth[localX + GROUP_SIZE] = depthTex[uint2(rightX, DTid.y)];
    }
    
    // Sync
    GroupMemoryBarrierWithGroupSync();
    
    if (DTid.x >= Constants.textureWidth)
        return;
    
    // Bilateral blur
    float centerDepth = cacheDepth[localX];
    float totalAO = 0.0f;
    float totalWeight = 0.0f;
    
    [unroll]
    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        float neighborAO = cacheAO[localX + i];
        float neighborDepth = cacheDepth[localX + i];
        float spatialWeight = Weights[i + RADIUS];
        
        float depthDiff = abs(centerDepth - neighborDepth);
        float rangeWeight = exp(-depthDiff * 100.0f);
        
        float finalWeight = spatialWeight * rangeWeight;
        
        totalAO += neighborAO * finalWeight;
        totalWeight += finalWeight;
    }
    
    // Normalize because border pixels cual have a totalWeight != 1
    outputAO[DTid.xy] = totalAO / max(totalWeight, 0.0001f);
}
    
