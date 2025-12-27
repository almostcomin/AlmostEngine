#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dxgi.h>

#include "Core/Math.h"
#include "RHI/Format.h"

namespace st::gfx
{
    enum class VertexAttribute
    {
        Position,
        PrevPosition,
        TexCoord1,
        TexCoord2,
        Normal,
        Tangent,
        Transform,
        PrevTransform,
        JointIndices,
        JointWeights,
        CurveRadius,

        Count
    };

    static constexpr uint32_t InstanceFlags_CurveDisjointOrthogonalTriangleStrips = 0x00000001u;

    struct InstanceData
    {
        uint32_t flags;
        uint32_t firstGeometryInstanceIndex; // index into global list of geometry instances. 
        // foreach (Instance)
        //     foreach(Geo) index++
        uint32_t firstGeometryIndex;         // index into global list of geometries. 
        // foreach(Mesh)
        //     foreach(Geo) index++
        uint32_t numGeometries;

        float4x4 transform;
        float4x4 prevTransform;

        bool IsCurveDOTS() { return (flags & InstanceFlags_CurveDisjointOrthogonalTriangleStrips) != 0; }
    };

	rhi::Format GetFormat(DXGI_FORMAT format);

	uint32_t BitsPerPixel(rhi::Format format);

//    nvrhi::VertexAttributeDesc GetVertexAttributeDesc(VertexAttribute attribute, const char* name, uint32_t bufferIndex);

} // namespace st::gfx