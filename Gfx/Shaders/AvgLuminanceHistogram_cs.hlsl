#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// http://www.alextardif.com/HistogramLuminance.html

#define NUM_HISTOGRAM_BINS 256

ConstantBuffer<interop::AvgLuminanceHistogramConstants> Constants : register(b0);

groupshared float HistogramShared[NUM_HISTOGRAM_BINS];

[RootSignature(BindlessRootSignature)]
[numthreads(NUM_HISTOGRAM_BINS, 1, 1)]
void main(uint groupIndex : SV_GroupIndex)
{
    RWByteAddressBuffer inputHistogram = ResourceDescriptorHeap[Constants.inputHistogramBufferDI];
    RWTexture2D<float> luminanceOutput = ResourceDescriptorHeap[Constants.outputAvgLuminanceTextureDI];
    RWStructuredBuffer<interop::TonemappingStatsBuffer> statsBuffer = ResourceDescriptorHeap[Constants.outputStatsBufferDI];
    
    float countForThisBin = (float)inputHistogram.Load(groupIndex * 4);
    HistogramShared[groupIndex] = countForThisBin * (float)groupIndex;
    
    GroupMemoryBarrierWithGroupSync();

    // 128 -> 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1
    [unroll]
    for (uint histogramSampleIndex = (NUM_HISTOGRAM_BINS >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1)
    {
        if (groupIndex < histogramSampleIndex)
        {
            HistogramShared[groupIndex] += HistogramShared[groupIndex + histogramSampleIndex];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0)
    {
        // HistogramShared[0] = SUM(count_i * i)
        // countForThisBin = cpunt of bin 0, discarded pixels
        // Substract 1 because bin 0 is ignored
        float avgBin = (HistogramShared[0] / max((float)Constants.pixelCount - countForThisBin, 1.0)) - 1.0;
        
        float targetLuminance = exp2(((avgBin / 254.0) * Constants.logLuminanceRange) + Constants.minLogLuminance);
        float oldLuminance = luminanceOutput[uint2(0, 0)];
        float newLuminance = oldLuminance + (targetLuminance - oldLuminance) * (1 - exp(-Constants.timeDelta * Constants.tau));
        
        luminanceOutput[uint2(0, 0)] = newLuminance;
        statsBuffer[0].avgLuminance = newLuminance;
        statsBuffer[0].avgBin = avgBin;
    }
}