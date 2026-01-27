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
    
    float countForThisBin = (float)luminanceOutput.Load(groupIndex * 4);
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
        float weightedLogAverage = (HistogramShared[0].x / max((float)Constants.pixelCount - countForThisBin, 1.0)) - 1.0;
        float weightedAverageLuminance = exp2(((weightedLogAverage / 254.0) * Constants.logLuminanceRange) + Constants.minLogLuminance);
        float luminanceLastFrame = luminanceOutput[uint2(0, 0)];
        float adaptedLuminance = luminanceLastFrame + (weightedAverageLuminance - luminanceLastFrame) * (1 - exp(-Constants.timeDelta * Constants.tau));
        luminanceOutput[uint2(0, 0)] = adaptedLuminance;
    }
}