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

    NoiseHeightmapSource(const Params& params, float scaleH, float cellSize, const std::string& name) : 
        m_Params{ params }, m_ScaleH{ scaleH }, m_CellSize{ cellSize }, m_Name{ name }
    {}

    const Params& GetParams() const { return m_Params; }
    void SetParams(const Params& p) { m_Params = p; }

    float GetHeight(const uint2& p) const override;
    float Sample(const float2& uv) const override;

    bool IsTileable() const override { return true; }
    float2 GetNormalizedSize() const override { return float2{ 1.f, 1.f }; }
    float2 GetHeightRange() const override { return float2{ 0.f, m_ScaleH }; }

    bool InfiniteDataResolution() const override { return true; }
    uint2 GetDataResolution() const override { return { UINT32_MAX, UINT32_MAX }; }

    bool HasCellSize() const override { return true; }
    float GetCellSize() const override { return m_CellSize; }

    Type GetType() const override { return Type::Noise; }
    const std::string& GetName() const override { return m_Name; }

private:

    Params m_Params;

    float m_ScaleH;
    float m_CellSize;

    std::string m_Name;
};
}