#pragma once

namespace st::rapi
{

struct Viewport
{
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;

    Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f) {}

    Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f) {}

    Viewport(float _minX, float _maxX, float _minY, float _maxY, float _minZ, float _maxZ)
        : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY), minZ(_minZ), maxZ(_maxZ)
    {
    }

    bool operator ==(const Viewport& b) const
    {
        return minX == b.minX
            && minY == b.minY
            && minZ == b.minZ
            && maxX == b.maxX
            && maxY == b.maxY
            && maxZ == b.maxZ;
    }
    bool operator !=(const Viewport& b) const { return !(*this == b); }

    [[nodiscard]] float width() const { return maxX - minX; }
    [[nodiscard]] float height() const { return maxY - minY; }
};

} // namespace st::rapi