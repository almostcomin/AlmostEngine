#pragma once

#include <nvrhi/nvrhi.h>
#include <filesystem>

namespace st::gfx
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
    size_t rowPitch = 0;
    size_t depthPitch = 0;
    ptrdiff_t dataOffset = 0;
    size_t dataSize = 0;
};

struct TextureInfo
{
    nvrhi::Format format = nvrhi::Format::UNKNOWN;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t arraySize = 1;
    uint32_t mipLevels = 1;
    nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Unknown;
    std::string debugName;

    // dataLayout[ArraySlice][MipLevel]
    std::vector<std::vector<TextureSubresourceInfo>> dataLayout;
};

} // namespace st::gfx
