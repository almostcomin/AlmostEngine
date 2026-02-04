#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// http://www.alextardif.com/HistogramLuminance.html

#define NUM_HISTOGRAM_BINS 256
#define GROUP_X 16
#define GROUP_Y 16

ConstantBuffer<interop::BuildLuminanceHistogramConstants> Constants : register(b0);

float GetLuminance(float3 color)
{
    return dot(color, float3(0.2127f, 0.7152f, 0.0722f));
}

uint LuminanceToHistogramBin(float luminance)
{
    if (luminance < 0.001)
    {
        return 0;
    }
    
    float logLuminance = saturate((log2(luminance) - Constants.minLogLuminance) * Constants.oneOverLogLuminanceRange);
    return (uint)(logLuminance * 254.0 + 1.0);
}

groupshared uint HistogramShared[NUM_HISTOGRAM_BINS];
groupshared uint minLuminanceShared[NUM_HISTOGRAM_BINS];
groupshared uint maxLuminanceShared[NUM_HISTOGRAM_BINS];

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_X, GROUP_X, 1)]
void main(uint groupIndex : SV_GroupIndex, uint3 threadID : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWByteAddressBuffer outputHistogram = ResourceDescriptorHeap[Constants.outputHistogramBufferDI];
    RWByteAddressBuffer statsBuffer = ResourceDescriptorHeap[Constants.outputStatsBufferDI]; // TonemappingStatsBuffer
    
    uint2 pixelPos = threadID.xy + Constants.viewBegin.xy;
    bool valid = all(pixelPos.xy < Constants.viewEnd.xy);
    
    HistogramShared[groupIndex] = 0;
    minLuminanceShared[groupIndex] = 0x7F7FFFFF; // FLT_MAX binary representation
    maxLuminanceShared[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync(); // ----------------------------
    
    if (valid)
    {
        float3 color = inputTexture[pixelPos].xyz;
        float luminance = GetLuminance(color);
        uint binIdx = LuminanceToHistogramBin(luminance);
        InterlockedAdd(HistogramShared[binIdx], 1);
        
        uint luminanceAsUint = asuint(luminance);
        InterlockedMin(minLuminanceShared[groupIndex], luminanceAsUint);
        InterlockedMax(maxLuminanceShared[groupIndex], luminanceAsUint);
    }
    
    GroupMemoryBarrierWithGroupSync(); // ----------------------------
    
    outputHistogram.InterlockedAdd(groupIndex * 4, HistogramShared[groupIndex]);

    // Parallel reduction
    if (groupIndex == 0)
    {
        uint maxLum = maxLuminanceShared[0];
        uint minLum = minLuminanceShared[0];
        
        for (uint i = 1; i < NUM_HISTOGRAM_BINS; i++)
        {
            maxLum = max(maxLum, maxLuminanceShared[i]);
            minLum = min(minLum, minLuminanceShared[i]);
        }
        
        // Update stats buffer global. 
        // Be aware that statsBuffer is TonemappingStatsBuffer, minLuminance is at offset 0 and maxLuminance at offset 4
        uint originalMin;
        statsBuffer.InterlockedMin(0, asuint(minLum), originalMin);
        uint originalMax;
        statsBuffer.InterlockedMax(4, asuint(maxLum), originalMax);
    }
}