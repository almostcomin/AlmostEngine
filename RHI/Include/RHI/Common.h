#pragma once

namespace st::rhi
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

};