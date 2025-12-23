#pragma once

namespace st::rapi
{

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