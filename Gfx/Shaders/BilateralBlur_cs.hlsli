#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

#define GROUP_SIZE 128
#define RADIUS 4

ConstantBuffer<interop::BilaterialBlurConstants> Constants : register(b0);

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
    Texture2D<float> inputTex = ResourceDescriptorHeap[Constants.inputTextureDI];
    Texture2D<float> depthTex = ResourceDescriptorHeap[Constants.depthTextureDI]; // linear
    RWTexture2D<float> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    
#ifdef HORIZONTAL // each shader group process a row
    int globalIdx = min(DTid.x, Constants.textureWidth - 1);
    int dimLimit = Constants.textureWidth;
    #define MAKE_COORD(idx, fixed) int2(idx, fixed)
    #define FIXED_COORD DTid.y
#else // VERTICAL, each shader group process a column
    int globalIdx = min(DTid.x, Constants.textureHeight - 1);
    int dimLimit = Constants.textureHeight;
    #define MAKE_COORD(idx, fixed) int2(fixed, idx)
    #define FIXED_COORD DTid.y
#endif
    
    int localIdx = GTid.x + RADIUS;
    
    // Fill cache - center
    cacheAO[localIdx] = inputTex[MAKE_COORD(globalIdx, FIXED_COORD)];
    cacheDepth[localIdx] = depthTex[MAKE_COORD(globalIdx, FIXED_COORD)];
    // Fill cache - apron
    if (GTid.x < RADIUS)
    {
        // left apron
        int leftIdx = max(0, globalIdx - RADIUS);
        cacheAO[GTid.x] = inputTex[MAKE_COORD(leftIdx, FIXED_COORD)];
        cacheDepth[GTid.x] = depthTex[MAKE_COORD(leftIdx, FIXED_COORD)];
        
        // right apron
        int rightIdx = min(dimLimit - 1, globalIdx + GROUP_SIZE);
        cacheAO[localIdx + GROUP_SIZE] = inputTex[MAKE_COORD(rightIdx, FIXED_COORD)];
        cacheDepth[localIdx + GROUP_SIZE] = depthTex[MAKE_COORD(rightIdx, FIXED_COORD)];
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (DTid.x >= dimLimit)
        return;
    
    // Bilateral blur
    float centerDepth = cacheDepth[localIdx];
    float totalAO = 0.0f;
    float totalWeight = 0.0f;
    
    [unroll]
    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        float neighborAO = cacheAO[localIdx + i];
        float neighborDepth = cacheDepth[localIdx + i];
        float spatialWeight = Weights[i + RADIUS];
        
        float depthDiff = abs(centerDepth - neighborDepth);
        float rangeWeight = exp(-depthDiff * 100.0f);
        
        float finalWeight = spatialWeight * rangeWeight;
        
        totalAO += neighborAO * finalWeight;
        totalWeight += finalWeight;
    }
    
    // Normalize because border pixels could have a totalWeight != 1
    outputTex[MAKE_COORD(globalIdx, FIXED_COORD)] = totalAO / max(totalWeight, 0.0001f);
}
