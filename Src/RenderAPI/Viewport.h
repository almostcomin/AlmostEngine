#pragma once

#include "Core/static_vector.h"
#include "Core/Math/aabox.h"
#include "Core/Math.h"

namespace st::rapi
{

static constexpr uint32_t c_MaxViewports = 16;

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

    [[nodiscard]] float Width() const { return maxX - minX; }
    [[nodiscard]] float Height() const { return maxY - minY; }
};

struct ViewportState
{
    static_vector<Viewport, c_MaxViewports> viewports;
    static_vector<st::math::aabox2i, c_MaxViewports> scissorRects;

    ViewportState& AddViewport(const Viewport& v) { viewports.push_back(v); return *this; }
    ViewportState& AddScissorRect(const st::math::aabox2i& r) { scissorRects.push_back(r); return *this; }
    ViewportState& AddViewportAndScissorRect(const Viewport& v) {
        return AddViewport(v).AddScissorRect(st::math::aabox2i{ int2{v.minX, v.minY}, int2{v.maxX, v.maxY} });
    }
};

} // namespace st::rapi