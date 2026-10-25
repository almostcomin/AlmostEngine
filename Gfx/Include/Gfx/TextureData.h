#pragma once

#include "RHI/Format.h"
#include "RHI/Texture.h"
#include "Core/Blob.h"

namespace alm::gfx
{

enum class TextureAlphaMode
{
    UNKNOWN = 0,
    STRAIGHT = 1,
    PREMULTIPLIED = 2,
    OPAQUE_ = 3,
    CUSTOM = 4,
};

struct TextureSubresourceInfo
{
    size_t RowPitch = 0;
    size_t DepthPitch = 0;
    ptrdiff_t DataOffset = 0;
    size_t DataSize = 0;
};

struct TextureData
{
    rhi::Format format = rhi::Format::UNKNOWN;
    uint32_t Width = 1;
    uint32_t Height = 1;
    uint32_t Depth = 1;
    uint32_t ArraySize = 1;
    uint32_t MipLevels = 1;
    rhi::TextureDimension Dimension = rhi::TextureDimension::Unknown;
    std::string DebugName;

    // dataLayout[ArraySlice][MipLevel]
    std::vector<std::vector<TextureSubresourceInfo>> DataLayout;
    alm::Blob Data;

    const TextureSubresourceInfo& GetSubresourceInfo(uint32_t arraySlice, uint32_t mipLevel) const
    {
        assert(arraySlice < ArraySize && mipLevel < MipLevels);
        return DataLayout[arraySlice][mipLevel];
    }
};

} // namespace st::gfx
