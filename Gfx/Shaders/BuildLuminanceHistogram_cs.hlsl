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

uint ColorToHistogramBin(float3 color)
{
    float luminance = GetLuminance(color);    
    if (luminance < 0.001)
    {
        return 0;
    }
    
    float logLuminance = saturate((log2(luminance) - Constants.minLogLuminance) * Constants.oneOverLogLuminanceRange);
    return (uint)(logLuminance * 254.0 + 1.0);
}

groupshared uint HistogramShared[NUM_HISTOGRAM_BINS];

[RootSignature(BindlessRootSignature)]
[numthreads(GROUP_X, GROUP_X, 1)]
void main(uint groupIndex : SV_GroupIndex, uint3 threadID : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWByteAddressBuffer outputHistogram = ResourceDescriptorHeap[Constants.outputHistogramBufferDI];
    
    uint2 pixelPos = threadID.xy + Constants.viewBegin.xy;
    bool valid = all(pixelPos.xy < Constants.viewEnd.xy);
    
    HistogramShared[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync(); // ----------------------------
    
    if (valid)
    {
        float3 color = inputTexture[pixelPos].xyz;
        uint binIdx = ColorToHistogramBin(color);
        InterlockedAdd(HistogramShared[binIdx], 1);
    }
    
    GroupMemoryBarrierWithGroupSync(); // ----------------------------
    
    outputHistogram.InterlockedAdd(groupIndex * 4, HistogramShared[groupIndex]);
}