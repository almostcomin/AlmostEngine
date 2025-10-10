#include "Gfx/TextureLoader.h"
#include <nvrhi/d3d12.h>
#include "Core/File.h"
#include "Core/Log.h"

#include <DirectXTex.h>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
    struct FormatMapping
    {
        nvrhi::Format nvrhiFormat;
        DXGI_FORMAT dxgiFormat;
        uint32_t bitsPerPixel;
    };

    const FormatMapping g_FormatMappings[] = {
        { nvrhi::Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                0 },
        { nvrhi::Format::R8_UINT,              DXGI_FORMAT_R8_UINT,                8 },
        { nvrhi::Format::R8_SINT,              DXGI_FORMAT_R8_SINT,                8 },
        { nvrhi::Format::R8_UNORM,             DXGI_FORMAT_R8_UNORM,               8 },
        { nvrhi::Format::R8_SNORM,             DXGI_FORMAT_R8_SNORM,               8 },
        { nvrhi::Format::RG8_UINT,             DXGI_FORMAT_R8G8_UINT,              16 },
        { nvrhi::Format::RG8_SINT,             DXGI_FORMAT_R8G8_SINT,              16 },
        { nvrhi::Format::RG8_UNORM,            DXGI_FORMAT_R8G8_UNORM,             16 },
        { nvrhi::Format::RG8_SNORM,            DXGI_FORMAT_R8G8_SNORM,             16 },
        { nvrhi::Format::R16_UINT,             DXGI_FORMAT_R16_UINT,               16 },
        { nvrhi::Format::R16_SINT,             DXGI_FORMAT_R16_SINT,               16 },
        { nvrhi::Format::R16_UNORM,            DXGI_FORMAT_R16_UNORM,              16 },
        { nvrhi::Format::R16_SNORM,            DXGI_FORMAT_R16_SNORM,              16 },
        { nvrhi::Format::R16_FLOAT,            DXGI_FORMAT_R16_FLOAT,              16 },
        { nvrhi::Format::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         16 },
        { nvrhi::Format::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           16 },
        { nvrhi::Format::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         16 },
        { nvrhi::Format::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_UINT,          32 },
        { nvrhi::Format::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_SINT,          32 },
        { nvrhi::Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_UNORM,         32 },
        { nvrhi::Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_SNORM,         32 },
        { nvrhi::Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_UNORM,         32 },
        { nvrhi::Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    32 },
        { nvrhi::Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    32 },
        { nvrhi::Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_UNORM,      32 },
        { nvrhi::Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        32 },
        { nvrhi::Format::RG16_UINT,            DXGI_FORMAT_R16G16_UINT,            32 },
        { nvrhi::Format::RG16_SINT,            DXGI_FORMAT_R16G16_SINT,            32 },
        { nvrhi::Format::RG16_UNORM,           DXGI_FORMAT_R16G16_UNORM,           32 },
        { nvrhi::Format::RG16_SNORM,           DXGI_FORMAT_R16G16_SNORM,           32 },
        { nvrhi::Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_FLOAT,           32 },
        { nvrhi::Format::R32_UINT,             DXGI_FORMAT_R32_UINT,               32 },
        { nvrhi::Format::R32_SINT,             DXGI_FORMAT_R32_SINT,               32 },
        { nvrhi::Format::R32_FLOAT,            DXGI_FORMAT_R32_FLOAT,              32 },
        { nvrhi::Format::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_UINT,      64 },
        { nvrhi::Format::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_SINT,      64 },
        { nvrhi::Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_FLOAT,     64 },
        { nvrhi::Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_UNORM,     64 },
        { nvrhi::Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_SNORM,     64 },
        { nvrhi::Format::RG32_UINT,            DXGI_FORMAT_R32G32_UINT,            64 },
        { nvrhi::Format::RG32_SINT,            DXGI_FORMAT_R32G32_SINT,            64 },
        { nvrhi::Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_FLOAT,           64 },
        { nvrhi::Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_UINT,         96 },
        { nvrhi::Format::RGB32_SINT,           DXGI_FORMAT_R32G32B32_SINT,         96 },
        { nvrhi::Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT,        96 },
        { nvrhi::Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_UINT,      128 },
        { nvrhi::Format::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_SINT,      128 },
        { nvrhi::Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_FLOAT,     128 },
        { nvrhi::Format::D16,                  DXGI_FORMAT_R16_UNORM,              16 },
        { nvrhi::Format::D24S8,                DXGI_FORMAT_R24_UNORM_X8_TYPELESS,  32 },
        { nvrhi::Format::X24G8_UINT,           DXGI_FORMAT_X24_TYPELESS_G8_UINT,   32 },
        { nvrhi::Format::D32,                  DXGI_FORMAT_R32_FLOAT,              32 },
        { nvrhi::Format::D32S8,                DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 64 },
        { nvrhi::Format::X32G8_UINT,           DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  64 },
        { nvrhi::Format::BC1_UNORM,            DXGI_FORMAT_BC1_UNORM,              4 },
        { nvrhi::Format::BC1_UNORM_SRGB,       DXGI_FORMAT_BC1_UNORM_SRGB,         4 },
        { nvrhi::Format::BC2_UNORM,            DXGI_FORMAT_BC2_UNORM,              8 },
        { nvrhi::Format::BC2_UNORM_SRGB,       DXGI_FORMAT_BC2_UNORM_SRGB,         8 },
        { nvrhi::Format::BC3_UNORM,            DXGI_FORMAT_BC3_UNORM,              8 },
        { nvrhi::Format::BC3_UNORM_SRGB,       DXGI_FORMAT_BC3_UNORM_SRGB,         8 },
        { nvrhi::Format::BC4_UNORM,            DXGI_FORMAT_BC4_UNORM,              4 },
        { nvrhi::Format::BC4_SNORM,            DXGI_FORMAT_BC4_SNORM,              4 },
        { nvrhi::Format::BC5_UNORM,            DXGI_FORMAT_BC5_UNORM,              8 },
        { nvrhi::Format::BC5_SNORM,            DXGI_FORMAT_BC5_SNORM,              8 },
        { nvrhi::Format::BC6H_UFLOAT,          DXGI_FORMAT_BC6H_UF16,              8 },
        { nvrhi::Format::BC6H_SFLOAT,          DXGI_FORMAT_BC6H_SF16,              8 },
        { nvrhi::Format::BC7_UNORM,            DXGI_FORMAT_BC7_UNORM,              8 },
        { nvrhi::Format::BC7_UNORM_SRGB,       DXGI_FORMAT_BC7_UNORM_SRGB,         8 },
    };

    nvrhi::Format GetFormat(DXGI_FORMAT format)
    {
        for (const FormatMapping mapping : g_FormatMappings)
        {
            if (mapping.dxgiFormat == format)
            {
                return mapping.nvrhiFormat;
            }
        }

        LOG_ERROR("Format unknown: [{:#x}]", (uint32_t)format);
        return nvrhi::Format::UNKNOWN;
    };

    static uint32_t BitsPerPixel(nvrhi::Format format)
    {
        const FormatMapping& mapping = g_FormatMappings[static_cast<uint32_t>(format)];
        assert(mapping.nvrhiFormat == format);

        return mapping.bitsPerPixel;
    }

    void GetSurfaceInfo(size_t width, size_t height, nvrhi::Format fmt, uint32_t bitsPerPixel,
        size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows)
    {
        size_t numBytes = 0;
        size_t rowBytes = 0;
        size_t numRows = 0;

        bool bc = false;
        size_t bpe = 0;
        switch (fmt)
        {
        case nvrhi::Format::BC1_UNORM:
        case nvrhi::Format::BC1_UNORM_SRGB:
        case nvrhi::Format::BC4_UNORM:
        case nvrhi::Format::BC4_SNORM:
            bc = true;
            bpe = 8;
            break;

        case nvrhi::Format::BC2_UNORM:
        case nvrhi::Format::BC2_UNORM_SRGB:
        case nvrhi::Format::BC3_UNORM:
        case nvrhi::Format::BC3_UNORM_SRGB:
        case nvrhi::Format::BC5_UNORM:
        case nvrhi::Format::BC5_SNORM:
        case nvrhi::Format::BC6H_UFLOAT:
        case nvrhi::Format::BC6H_SFLOAT:
        case nvrhi::Format::BC7_UNORM:
        case nvrhi::Format::BC7_UNORM_SRGB:
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

    size_t FillTextureInfoOffsets(st::gfx::TextureInfo& textureInfo, size_t dataSize, ptrdiff_t dataOffset)
    {
        //textureInfo.originalBitsPerPixel = BitsPerPixel(textureInfo.format);

        textureInfo.dataLayout.resize(textureInfo.arraySize);
        for (uint32_t arraySlice = 0; arraySlice < textureInfo.arraySize; arraySlice++)
        {
            size_t w = textureInfo.width;
            size_t h = textureInfo.height;
            size_t d = textureInfo.depth;

            std::vector<st::gfx::TextureSubresourceInfo>& sliceData = textureInfo.dataLayout[arraySlice];
            sliceData.resize(textureInfo.mipLevels);

            for (uint32_t mipLevel = 0; mipLevel < textureInfo.mipLevels; mipLevel++)
            {
                size_t NumBytes = 0;
                size_t RowBytes = 0;
                size_t NumRows = 0;
                GetSurfaceInfo(w, h, textureInfo.format, BitsPerPixel(textureInfo.format), &NumBytes, &RowBytes, &NumRows);

                st::gfx::TextureSubresourceInfo& levelData = sliceData[mipLevel];
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

    std::expected<nvrhi::TextureHandle, std::string> CreateTextureFromDDSMetadata(
        const DirectX::TexMetadata& metadata, const std::string& path, nvrhi::DeviceHandle device)
    {
        ID3D12Resource* d3d12Texture = nullptr;
        HRESULT hr = DirectX::CreateTexture(device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device), metadata, &d3d12Texture);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Error creating DDS texture, file '{}', hr [{:#x}]", path, hr));
        }

        D3D12_RESOURCE_DESC d3d12Desc = d3d12Texture->GetDesc();

        std::filesystem::path fullPath = path;
        std::string debugName = fullPath.filename().string();

        nvrhi::TextureDesc nvrhiDesc;
        nvrhiDesc.width = d3d12Desc.Width;
        nvrhiDesc.height = d3d12Desc.Height;
        nvrhiDesc.depth = 1; // TODO
        nvrhiDesc.arraySize = d3d12Desc.DepthOrArraySize;
        nvrhiDesc.mipLevels = d3d12Desc.MipLevels;
        nvrhiDesc.format = GetFormat(d3d12Desc.Format);
        nvrhiDesc.dimension = nvrhi::TextureDimension::Texture2D; // TODO
        nvrhiDesc.debugName = debugName.c_str();
        nvrhiDesc.isShaderResource = true;
        nvrhiDesc.isRenderTarget = false;
        nvrhiDesc.isUAV = false;
        nvrhiDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        nvrhiDesc.keepInitialState = false;

        nvrhi::TextureHandle nvrhiTexture = device->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(d3d12Texture), nvrhiDesc);
        return nvrhiTexture;
    };
} // anonymous namespace

std::expected<std::pair<st::gfx::TextureInfo, std::unique_ptr<DirectX::ScratchImage>>, std::string> 
st::gfx::LoadDDSTexture(const std::string& path)
{
	st::fs::File file(path);
	if (!file.IsOpen())
	{
		return std::unexpected(std::format("File '{}' not found", path));
	}
	
	st::Blob fileData;
	if (auto readResult = file.Read(); readResult)
	{
		fileData = std::move(*readResult);
	}
	else
	{
		return std::unexpected(std::format("Error reading file '{}'", path));
	}

	DirectX::TexMetadata metadata;
    auto image = std::make_unique<DirectX::ScratchImage>();
	HRESULT hr = DirectX::LoadFromDDSMemory((std::byte*)fileData.data(), fileData.size(), DirectX::DDS_FLAGS_NONE, &metadata, *image);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("Error loading DDS image, file '{}', hr [{:#x}]", path, hr));
	}

	st::gfx::TextureInfo texInfo;
    texInfo.format = GetFormat(metadata.format);
    assert(texInfo.format != nvrhi::Format::UNKNOWN);
	texInfo.width = metadata.width;
	texInfo.height = metadata.height;
	texInfo.mipLevels = metadata.mipLevels;
	texInfo.depth = 1;
	texInfo.arraySize = 1;
	//texInfo.alphaMode = GetAlphaMode(header);
    
    switch (metadata.dimension)
    {
    case DirectX::TEX_DIMENSION_TEXTURE1D:
        texInfo.dimension = metadata.arraySize > 1 ? nvrhi::TextureDimension::Texture1DArray : nvrhi::TextureDimension::Texture1D;
        texInfo.arraySize = metadata.arraySize;
        break;

    case DirectX::TEX_DIMENSION_TEXTURE2D:
        if (metadata.IsCubemap())
        {
            texInfo.dimension = metadata.arraySize > 1 ? nvrhi::TextureDimension::TextureCubeArray : nvrhi::TextureDimension::TextureCube;
            texInfo.arraySize = metadata.arraySize * 6;
        }
        else
        {
            texInfo.dimension = metadata.arraySize > 1 ? nvrhi::TextureDimension::Texture2DArray : nvrhi::TextureDimension::Texture2D;
            texInfo.arraySize = metadata.arraySize;
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE3D:
        assert(metadata.IsVolumemap());
        texInfo.depth = metadata.depth;
        texInfo.dimension = nvrhi::TextureDimension::Texture3D;
        break;
    }

    FillTextureInfoOffsets(texInfo, image->GetPixelsSize(), 0);

    return std::pair<st::gfx::TextureInfo, std::unique_ptr<DirectX::ScratchImage>> { texInfo, std::move(image) };
}

std::expected<std::pair<st::gfx::TextureInfo, st::Blob>, std::string> 
st::gfx::LoadImageTexture(const std::string& path)
{
	st::fs::File file(path);
	if (!file.IsOpen())
	{
		return std::unexpected(std::format("File '{}' not found", path));
	}

	st::Blob fileData;
	if (auto readResult = file.Read(); readResult)
	{
		fileData = std::move(*readResult);
	}
	else
	{
		return std::unexpected(std::format("Error reading file '{}'", path));
	}

	int width = 0, height = 0, originalChannels = 0;
	if (!stbi_info_from_memory((stbi_uc*)fileData.data(), fileData.size(), &width, &height, &originalChannels))
	{

		return std::unexpected(std::format("Couldn't process image header for texture '{}'", path));
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
		return std::unexpected(std::format("Couldn't load generic texture '{}'", path));
	}

	st::gfx::TextureInfo texInfo;
	//texInfo.originalBitsPerPixel = static_cast<uint32_t>(originalChannels) * (is_hdr ? 32 : 8);
	texInfo.width = static_cast<uint32_t>(width);
	texInfo.height = static_cast<uint32_t>(height);
	texInfo.mipLevels = 1;
	texInfo.dimension = nvrhi::TextureDimension::Texture2D;

	texInfo.dataLayout.resize(1);
	texInfo.dataLayout[0].resize(1);
	texInfo.dataLayout[0][0].dataOffset = 0;
	texInfo.dataLayout[0][0].rowPitch = static_cast<size_t>(width * bytesPerPixel);
	texInfo.dataLayout[0][0].dataSize = static_cast<size_t>(width * height * bytesPerPixel);

	st::Blob blob{ 
		(char*)bitmap, (size_t)(width * height * originalChannels), [](void* ptr) { stbi_image_free(ptr); } };

	switch (channels)
	{
	case 1:
		texInfo.format = is_hdr ? nvrhi::Format::R32_FLOAT : nvrhi::Format::R8_UNORM;
		break;
	case 2:
		texInfo.format = is_hdr ? nvrhi::Format::RG32_FLOAT : nvrhi::Format::RG8_UNORM;
		break;
	case 4:
		texInfo.format = is_hdr ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
		break;
	default:
		return std::unexpected(std::format("Unsupported number of components ({}) for texture '{}'", channels, path));
	}

	return std::pair<st::gfx::TextureInfo, st::Blob>{ texInfo, std::move(blob) };
}