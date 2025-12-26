#include "RHI/Texture.h"

st::rhi::MipLevel st::rhi::TextureSubresourceSet::GetLastMipLevel(const TextureDesc& desc) const
{
    if (numMipLevels == AllMipLevels)
    {
        return desc.mipLevels - 1;
    }
    else
    {
        return std::min(baseMipLevel + numMipLevels, desc.mipLevels) - 1;
    }
}

st::rhi::ArraySlice st::rhi::TextureSubresourceSet::GetLastArraySlice(const TextureDesc& desc) const
{
    if (numArraySlices == AllArraySlices)
    {
        return desc.arraySize - 1;
    }
    else
    {
        return std::min(baseArraySlice + numArraySlices, desc.arraySize) - 1;
    }
}

size_t st::rhi::TextureSubresourceSet::GetNumMipLevels(const TextureDesc& desc) const
{
    return GetLastMipLevel(desc) - baseMipLevel + 1;
}

size_t st::rhi::TextureSubresourceSet::GetNumArraySlices(const TextureDesc& desc) const
{
    return GetLastArraySlice(desc) - baseArraySlice + 1;
}

void st::rhi::TextureSubresourceSet::Resolve(const TextureDesc& desc)
{
    numMipLevels = GetNumMipLevels(desc);
    numArraySlices = GetNumArraySlices(desc);
}

bool st::rhi::TextureSubresourceSet::IsEntireTexture(const st::rhi::TextureDesc& desc) const
{
    if (baseMipLevel > 0u || baseMipLevel + numMipLevels < desc.mipLevels)
        return false;

    switch (desc.dimension)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case TextureDimension::Texture1DArray:
    case TextureDimension::Texture2DArray:
    case TextureDimension::TextureCube:
    case TextureDimension::TextureCubeArray:
    case TextureDimension::Texture2DMSArray:
        if (baseArraySlice > 0u || baseArraySlice + numArraySlices < desc.arraySize)
            return false;
    default:
        return true;
    }
}