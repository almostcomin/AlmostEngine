#pragma once

#include "Gfx/HeightmapSource.h"

namespace alm::gfx
{

class ImageHeightmapSource : public IHeightmapSource
{
public:

    enum class EdgeMode
    {
        Clamp,
        Wrap
    };

public:

    ImageHeightmapSource(EdgeMode edgeMode = EdgeMode::Clamp) : m_EdgeMode{ edgeMode }
    {}

    bool Load(const std::string& path);

    // IHeightmapSource interface
    float Sample(const float2& uv) const override;
    bool IsTileable() const override { return m_EdgeMode == EdgeMode::Wrap; }
    float2 GetNormalizedSize() const override;
    float2 GetHeightRange() const override;

private:

    void ComputeHeightRange();

    EdgeMode m_EdgeMode = EdgeMode::Clamp;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    float2 m_HeightRange;

    std::vector<float> m_Data;

};

} // namespace alm::gfx