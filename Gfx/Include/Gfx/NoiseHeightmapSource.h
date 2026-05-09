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

private:

    Params m_Params;
};
}