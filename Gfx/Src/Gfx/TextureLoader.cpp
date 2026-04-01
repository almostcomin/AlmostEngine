#include "Gfx/GfxPCH.h"
#include "Gfx/TextureLoader.h"
#include "Core/File.h"
#include "Core/Log.h"
#include "Gfx/Util.h"

#include <DirectXTex.h>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
    void GetSurfaceInfo(size_t width, size_t height, alm::rhi::Format fmt, uint32_t bitsPerPixel,
        size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows)
    {
        size_t numBytes = 0;
        size_t rowBytes = 0;
        size_t numRows = 0;

        bool bc = false;
        size_t bpe = 0;
        switch (fmt)
        {
        case alm::rhi::Format::BC1_UNORM:
        case alm::rhi::Format::BC1_UNORM_SRGB:
        case alm::rhi::Format::BC4_UNORM:
        case alm::rhi::Format::BC4_SNORM:
            bc = true;
            bpe = 8;
            break;

        case alm::rhi::Format::BC2_UNORM:
        case alm::rhi::Format::BC2_UNORM_SRGB:
        case alm::rhi::Format::BC3_UNORM:
        case alm::rhi::Format::BC3_UNORM_SRGB:
        case alm::rhi::Format::BC5_UNORM:
        case alm::rhi::Format::BC5_SNORM:
        case alm::rhi::Format::BC6H_UFLOAT:
        case alm::rhi::Format::BC6H_SFLOAT:
        case alm::rhi::Format::BC7_UNORM:
        case alm::rhi::Format::BC7_UNORM_SRGB:
            bc = true;
            bpe = 16;
            break;

        default:
            break;
        }

        if (bc)
        {
            size_t numBlocksWide = 0;
            if (width > 0)
            {
                numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
            }
            size_t numBlocksHigh = 0;
            if (height > 0)
            {
                numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
            }
            rowBytes = numBlocksWide * bpe;
            numRows = numBlocksHigh;
            numBytes = rowBytes * numBlocksHigh;
        }
        else
        {
            rowBytes = (width * bitsPerPixel + 7) / 8; // round up to nearest byte
            numRows = height;
            numBytes = rowBytes * height;
        }

        if (outNumBytes)
        {
            *outNumBytes = numBytes;
        }
        if (outRowBytes)
        {
            *outRowBytes = rowBytes;
        }
        if (outNumRows)
        {
            *outNumRows = numRows;
        }
    }

    size_t FillTextureInfoOffsets(alm::gfx::TextureInfo& textureInfo, size_t dataSize, ptrdiff_t dataOffset)
    {
        //textureInfo.originalBitsPerPixel = BitsPerPixel(textureInfo.format);

        textureInfo.dataLayout.resize(textureInfo.arraySize);
        for (uint32_t arraySlice = 0; arraySlice < textureInfo.arraySize; arraySlice++)
        {
            size_t w = textureInfo.width;
            size_t h = textureInfo.height;
            size_t d = textureInfo.depth;

            std::vector<alm::gfx::TextureSubresourceInfo>& sliceData = textureInfo.dataLayout[arraySlice];
            sliceData.resize(textureInfo.mipLevels);

            for (uint32_t mipLevel = 0; mipLevel < textureInfo.mipLevels; mipLevel++)
            {
                size_t NumBytes = 0;
                size_t RowBytes = 0;
                size_t NumRows = 0;
                GetSurfaceInfo(w, h, textureInfo.format, alm::gfx::BitsPerPixel(textureInfo.format), &NumBytes, &RowBytes, &NumRows);

                alm::gfx::TextureSubresourceInfo& levelData = sliceData[mipLevel];
                levelData.dataOffset = dataOffset;
                levelData.dataSize = NumBytes;
                levelData.rowPitch = RowBytes;
                levelData.depthPitch = RowBytes * NumRows;

                dataOffset += NumBytes * d;
                assert(dataOffset < static_cast<ptrdiff_t>(dataSize));

                w = w >> 1;
                h = h >> 1;
                d = d >> 1;

                if (w == 0) w = 1;
                if (h == 0) h = 1;
                if (d == 0) d = 1;
            }
        }

        return dataOffset;
    }
} // anonymous namespace

std::expected<std::pair<alm::gfx::TextureInfo, alm::Blob>, std::string>
alm::gfx::LoadDDSTexture(const alm::WeakBlob& fileData)
{
	DirectX::TexMetadata metadata;
    auto image = std::make_unique<DirectX::ScratchImage>();
	HRESULT hr = DirectX::LoadFromDDSMemory((std::byte*)fileData.data(), fileData.size(), DirectX::DDS_FLAGS_NONE, &metadata, *image);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("Error loading DDS image, hr [{:#x}]", hr));
	}

	alm::gfx::TextureInfo texInfo;
    texInfo.format = GetFormat(metadata.format);
    assert(texInfo.format != alm::rhi::Format::UNKNOWN);
	texInfo.width = metadata.width;
	texInfo.height = metadata.height;
	texInfo.mipLevels = metadata.mipLevels;
	texInfo.depth = 1;
	texInfo.arraySize = 1;
	//texInfo.alphaMode = GetAlphaMode(header);
    
    switch (metadata.dimension)
    {
    case DirectX::TEX_DIMENSION_TEXTURE1D:
        texInfo.dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::Texture1DArray : alm::rhi::TextureDimension::Texture1D;
        texInfo.arraySize = metadata.arraySize;
        break;

    case DirectX::TEX_DIMENSION_TEXTURE2D:
        if (metadata.IsCubemap())
        {
            texInfo.dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::TextureCubeArray : alm::rhi::TextureDimension::TextureCube;
            texInfo.arraySize = metadata.arraySize * 6;
        }
        else
        {
            texInfo.dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::Texture2DArray : alm::rhi::TextureDimension::Texture2D;
            texInfo.arraySize = metadata.arraySize;
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE3D:
        assert(metadata.IsVolumemap());
        texInfo.depth = metadata.depth;
        texInfo.dimension = alm::rhi::TextureDimension::Texture3D;
        break;
    }

    FillTextureInfoOffsets(texInfo, image->GetPixelsSize(), 0);

    DirectX::ScratchImage* image_ptr = image.release();
    alm::Blob blob
        { (char*)image_ptr->GetPixels(), image_ptr->GetPixelsSize(), (char*)image_ptr, [](void* ptr) { delete (DirectX::ScratchImage*)ptr; } };

    return std::pair<alm::gfx::TextureInfo, alm::Blob> { texInfo, std::move(blob) };
}

std::expected<std::pair<alm::gfx::TextureInfo, alm::Blob>, std::string> 
alm::gfx::LoadImageTexture(const alm::WeakBlob& fileData, bool forceSRGB)
{
	int width = 0, height = 0, originalChannels = 0;
	if (!stbi_info_from_memory((stbi_uc*)fileData.data(), fileData.size(), &width, &height, &originalChannels))
	{
		return std::unexpected("Couldn't process image header for texture");
	}

	bool is_hdr = stbi_is_hdr_from_memory((stbi_uc*)fileData.data(), fileData.size());
	int channels = originalChannels == 3 ? 4 : originalChannels;
	int bytesPerPixel = channels * (is_hdr ? 4 : 1);

	unsigned char* bitmap;
	if (is_hdr)
	{
		float* floatmap = stbi_loadf_from_memory((stbi_uc*)fileData.data(), fileData.size(),
			&width, &height, &originalChannels, channels);
		bitmap = reinterpret_cast<unsigned char*>(floatmap);
	}
	else
	{
		bitmap = stbi_load_from_memory((stbi_uc*)fileData.data(), fileData.size(),
			&width, &height, &originalChannels, channels);
	}
	if (!bitmap)
	{
		return std::unexpected("Couldn't load generic texture");
	}

	alm::gfx::TextureInfo texInfo;
	//texInfo.originalBitsPerPixel = static_cast<uint32_t>(originalChannels) * (is_hdr ? 32 : 8);
	texInfo.width = static_cast<uint32_t>(width);
	texInfo.height = static_cast<uint32_t>(height);
	texInfo.mipLevels = 1;
	texInfo.dimension = alm::rhi::TextureDimension::Texture2D;

	texInfo.dataLayout.resize(1);
	texInfo.dataLayout[0].resize(1);
	texInfo.dataLayout[0][0].dataOffset = 0;
	texInfo.dataLayout[0][0].rowPitch = static_cast<size_t>(width * bytesPerPixel);
	texInfo.dataLayout[0][0].dataSize = static_cast<size_t>(width * height * bytesPerPixel);

	alm::Blob blob{ 
		(char*)bitmap, (size_t)(width * height * channels), (char*)bitmap, [](void* ptr) { stbi_image_free(ptr); } };

	switch (channels)
	{
	case 1:
		texInfo.format = is_hdr ? alm::rhi::Format::R32_FLOAT : alm::rhi::Format::R8_UNORM;
		break;
	case 2:
		texInfo.format = is_hdr ? alm::rhi::Format::RG32_FLOAT : alm::rhi::Format::RG8_UNORM;
		break;
	case 4:
		texInfo.format = is_hdr ? alm::rhi::Format::RGBA32_FLOAT :
            forceSRGB ? alm::rhi::Format::SRGBA8_UNORM : alm::rhi::Format::RGBA8_UNORM;
		break;
	default:
		return std::unexpected(std::format("Unsupported number of components ({})", channels));
	}

	return std::pair<alm::gfx::TextureInfo, alm::Blob>{ texInfo, std::move(blob) };
}