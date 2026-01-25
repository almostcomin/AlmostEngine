#ifndef __ACES_HLSLI__
#define __ACES_HLSLI__

float3 ACESFilm(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 ACESFilm_HDR(float3 x, float maxNits)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    
    // Normalize [0, maxNits]
    x = x / maxNits;
    
    // ACES curve
    float3 mapped = (x * (a * x + b)) / (x * (c * x + d) + e);
    
    // Re-space to maxNits
    return mapped * maxNits;
}

#endif //__ACES_HLSLI__