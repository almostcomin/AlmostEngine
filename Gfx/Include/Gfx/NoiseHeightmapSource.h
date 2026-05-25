#pragma once

#include "Gfx/HeightmapSource.h"

namespace alm::gfx
{
class NoiseHeightmapSource : public IHeightmapSource
{
public:

    struct Params
    {
        float Frequency = 1.0f;
        int   Octaves = 6;
        float OffsetX = 0.0f;
        float OffsetY = 0.0f;
    };

public:

    NoiseHeightmapSource(const Params& params) : m_Params{ params }
    {}

    float Sample(const float2& uv) const override;

    virtual bool InfiniteDataResolution() const { return true; }
    virtual uint2 GetDataResolution() const { return { UINT32_MAX, UINT32_MAX }; }

private:

    Params m_Params;
};
}