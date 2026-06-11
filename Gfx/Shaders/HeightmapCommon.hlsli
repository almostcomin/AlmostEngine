#ifndef __HEIGHTMAPCOMMON_HLSLI__
#define __HEIGHTMAPCOMMON_HLSLI__

float GetHeightmapMipBias(float2 pos2, uint edgeMask)
{
    float nB = edgeMask & 0x01;
    float sB = (edgeMask >> 1) & 0x01;
    float eB = (edgeMask >> 2) & 0x01;
    float wB = (edgeMask >> 3) & 0x01;
    
    return max(
        max(pos2.y == 1.0 ? nB : 0.0, pos2.y == 0.0 ? sB : 0.0),
        max(pos2.x == 1.0 ? eB : 0.0, pos2.x == 0.0 ? wB : 0.0));
}

#endif