#pragma once

namespace alm::rhi
{

using GpuVirtualAddress = uint64_t;

static constexpr uint32_t c_MaxRenderTargets = 8;

struct StorageRequirements
{
	size_t size;
	size_t alignment;
};

struct SubresourceCopyableRequirements
{
	size_t offset;
	size_t numRows;
	size_t rowSizeBytes;
	size_t rowStride;
};

enum class CopyMethod
{
	Buffer2Buffer,
	Buffer2Texture
};

enum class PrimitiveTopology
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    TriangleListWithAdjacency,
    TriangleStripWithAdjacency,
    PatchList
};

inline constexpr uint32_t GetPrimitiveCount(uint32_t indexCount, PrimitiveTopology topo)
{
    switch (topo)
    {
    case PrimitiveTopology::PointList:
        return indexCount;
    case PrimitiveTopology::LineList:
        return indexCount / 2;
    case PrimitiveTopology::LineStrip:
        return indexCount - 1;
    case PrimitiveTopology::TriangleList:
        return indexCount / 3;
    case PrimitiveTopology::TriangleStrip:
        return indexCount - 2;
    case PrimitiveTopology::TriangleFan:
        return indexCount - 2;
    case PrimitiveTopology::TriangleListWithAdjacency:
    case PrimitiveTopology::TriangleStripWithAdjacency:
    case PrimitiveTopology::PatchList:
    default:
        assert(0);
        return 0;
    }
}

} // namespace st::rhi