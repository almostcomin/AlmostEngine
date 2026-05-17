#pragma once

namespace alm::gfx
{
    class IHeightmapSource
    {
    public:

        virtual ~IHeightmapSource() = default;

        // Sample position p
        virtual float GetHeight(const uint2& p) const = 0;
        // uv in the normalized size range (GetNormalizedSize), returns height normalized in [0,1] range
        virtual float Sample(const float2& uv) const = 0;

        virtual bool IsTileable() const = 0;
        virtual uint2 GetDataSize() const = 0;
        virtual float2 GetNormalizedSize() const = 0;
        virtual float2 GetHeightRange() const = 0;

        virtual const std::string& GetName() const = 0;
    };
}