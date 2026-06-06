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
    float GetHeight(const uint2& p) const override;
    float Sample(const float2& uv) const override;

    bool IsTileable() const override { return m_EdgeMode == EdgeMode::Wrap; }
    float2 GetNormalizedSize() const override;
    float2 GetHeightRange() const override;

    virtual bool InfiniteDataResolution() const override { return false; }
    virtual uint2 GetDataResolution() const override { return { m_Width, m_Height }; }

    Type GetType() const override { return Type::Image; }
    const std::string& GetName() const override { return m_Name; }

private:

    void ComputeHeightRange();

    EdgeMode m_EdgeMode = EdgeMode::Clamp;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    float2 m_HeightRange;

    std::vector<float> m_Data;

    std::string m_Name = "<uninitialized>";
};

} // namespace alm::gfx