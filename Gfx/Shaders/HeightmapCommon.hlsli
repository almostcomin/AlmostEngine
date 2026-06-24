#ifndef __HEIGHTMAPCOMMON_HLSLI__
#define __HEIGHTMAPCOMMON_HLSLI__

// Keep in sync with alm::gfx::GBuffersRenderStage::DebugChannel
static const uint DebugChannel_Disabled = 0;
static const uint DebugChannel_Heightmap_Heights = 1;
static const uint DebugChannel_Heightmap_Slope = 2;
static const uint DebugChannel_Heightmap_Normals = 3;
static const uint DebugChannel_Heightmap_Connections = 4;
static const uint DebugChannel_Heightmap_Resolution = 5;
static const uint DebugChannel_Heightmap_Patches = 6;
static const uint DebugChannel_Heightmap_Contours = 7;

float GetHeightmapMipBias(float2 pos2, uint edgeMask)
{
    // bits 0-3 edge mode:		bit0 = N Low?,  bit1 = S Low?,  bit2 = E Low?,  bit3 = W Low?
    // bits 4-7 corner mode:	bit4 = NE Low?, bit5 = NW Low?, bit6 = SE Low?, bit7 = SW Low?    
    float nB = edgeMask & 0x01;
    float sB = (edgeMask >> 1) & 0x01;
    float eB = (edgeMask >> 2) & 0x01;
    float wB = (edgeMask >> 3) & 0x01;
    float neB = (edgeMask >> 4) & 0x01;
    float nwB = (edgeMask >> 5) & 0x01;
    float seB = (edgeMask >> 6) & 0x01;
    float swB = (edgeMask >> 7) & 0x01;
            
    float borderBias = max(
        max(pos2.y == 1.0 ? nB : 0.0, pos2.y == 0.0 ? sB : 0.0),
        max(pos2.x == 1.0 ? eB : 0.0, pos2.x == 0.0 ? wB : 0.0));
    
    float cornerBias;
    if      (pos2.x == 1.0 && pos2.y == 1.0) cornerBias = neB;
    else if (pos2.x == 0.0 && pos2.y == 1.0) cornerBias = nwB;
    else if (pos2.x == 1.0 && pos2.y == 0.0) cornerBias = seB;
    else if (pos2.x == 0.0 && pos2.y == 0.0) cornerBias = swB;
    else cornerBias = 0.0;
    
    return max(borderBias, cornerBias);
}

float3 GetHeightmapDebugConnectionColor(float2 pos2, uint edgeMask)
{
    const float3 normalColor = float3(0.25, 0.5, 1.0);
    const float3 borderColor = float3(0.0, 1.0, 0.0);
    const float3 lowColor = float3(1.0, 0.0, 0.0);

    if (pos2.x > 0.0 && pos2.y > 0.0 && pos2.x < 1.0 && pos2.y < 1.0)
        return normalColor;
    
    return GetHeightmapMipBias(pos2, edgeMask) == 0.0 ?
        borderColor : lowColor;
}

float GetContourLinesMask(float posY, float spacing, float pixelsWidth)
{
    float scaledY = posY / spacing;
    
    float rep = frac(scaledY);
    float distToEdge = min(rep, 1.0 - rep);
    
    float2 grad = float2(ddx(scaledY), ddy(scaledY));
    float pixelSize = length(grad);
    
    float distPixels = distToEdge / pixelSize;
    
    float borderMask = step(distPixels, pixelsWidth * 0.5);
    return borderMask;
}

#endif