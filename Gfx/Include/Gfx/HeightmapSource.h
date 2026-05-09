#pragma once

namespace alm::gfx
{
    class IHeightmapSource
    {
    public:

        virtual ~IHeightmapSource() = default;

        // uv in the normalized size range (GetNormalizedSize), returns height normalized in [0,1] range
        virtual float Sample(const float2& uv) const = 0;

        virtual bool IsTileable() const = 0;
        virtual float2 GetNormalizedSize() const = 0;
        virtual float2 GetHeightRange() const = 0;
    };
}