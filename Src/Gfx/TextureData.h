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

struct LoadedTexture
{
    nvrhi::TextureHandle texture;
    TextureAlphaMode alphaMode = TextureAlphaMode::UNKNOWN;
    uint32_t originalBitsPerPixel = 0;
    //DescriptorHandle bindlessDescriptor;
    std::string path;
    std::string mimeType;
};

struct TextureSubresourceData
{
    size_t rowPitch = 0;
    size_t depthPitch = 0;
    ptrdiff_t dataOffset = 0;
    size_t dataSize = 0;
};

struct TextureData : public LoadedTexture
{
    nvrhi::Format format = nvrhi::Format::UNKNOWN;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t arraySize = 1;
    uint32_t mipLevels = 1;
    nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Unknown;
    bool isRenderTarget = false;
    bool forceSRGB = false;

    // ArraySlice -> MipLevel -> TextureSubresourceData
    std::vector<std::vector<TextureSubresourceData>> dataLayout;
};

TextureSubresourceData GetTextureSubresourceInfo(

} // namespace st::gfx
