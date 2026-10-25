#include "Gfx/GfxPCH.h"
#include "Gfx/TextureLoader.h"
#include "Core/File.h"
#include "Gfx/Util.h"
#include "RHI/Device.h"
#include <filesystem>

// Need to include stb before DirectXTex because of weird compilation issue
// https://github.com/nothings/stb/issues/978
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <DirectXTex.h>

#include <ktx.h>
//#include <ktxtexture2.h> // Necesario para ktxTexture2_TranscodeBasis

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

    alm::rhi::Format MapTranscodeFormatToRHI(ktx_transcode_fmt_e fmt)
    {
        switch (fmt)
        {
        // Block compressed LDR
        case KTX_TTF_BC1_RGB:    return alm::rhi::Format::BC1_UNORM;
        case KTX_TTF_BC3_RGBA:   return alm::rhi::Format::BC3_UNORM;
        case KTX_TTF_BC4_R:      return alm::rhi::Format::BC4_UNORM;
        case KTX_TTF_BC5_RG:     return alm::rhi::Format::BC5_UNORM;
        case KTX_TTF_BC7_RGBA:   return alm::rhi::Format::BC7_UNORM;

        // Block compressed HDR
        case KTX_TTF_BC6HU_RGB:  return alm::rhi::Format::BC6H_UFLOAT;

        // No compression LDR
        case KTX_TTF_RGBA32:     return alm::rhi::Format::RGBA8_UNORM;
        case KTX_TTF_RGB565:     return alm::rhi::Format::B5G6R5_UNORM;

        // No compression HDR
        case KTX_TTF_RGBA_HALF:  return alm::rhi::Format::RGBA16_FLOAT;

        default:                 return alm::rhi::Format::UNKNOWN;
        }
    }

    // Maps the raw vkFormat field of a KTX2 file to an abstract format.
    // The case values are the VkFormat enum values from the Vulkan specification
    // (ABI stable), copied from the vendored Khronos headers, so no Vulkan SDK
    // include is required.
    alm::rhi::Format MapVkFormatToRHI(uint32_t vkFormat)
    {
        switch (vkFormat)
        {
        // 8-bit
        case 3:  return alm::rhi::Format::BGRA4_UNORM;   // VK_FORMAT_B4G4R4A4_UNORM_PACK16
        case 5:  return alm::rhi::Format::B5G6R5_UNORM;  // VK_FORMAT_B5G6R5_UNORM_PACK16
        case 9:  return alm::rhi::Format::R8_UNORM;      // VK_FORMAT_R8_UNORM
        case 10: return alm::rhi::Format::R8_SNORM;      // VK_FORMAT_R8_SNORM
        case 13: return alm::rhi::Format::R8_UINT;       // VK_FORMAT_R8_UINT
        case 14: return alm::rhi::Format::R8_SINT;       // VK_FORMAT_R8_SINT
        case 16: return alm::rhi::Format::RG8_UNORM;     // VK_FORMAT_R8G8_UNORM
        case 17: return alm::rhi::Format::RG8_SNORM;     // VK_FORMAT_R8G8_SNORM
        case 20: return alm::rhi::Format::RG8_UINT;      // VK_FORMAT_R8G8_UINT
        case 21: return alm::rhi::Format::RG8_SINT;      // VK_FORMAT_R8G8_SINT

        // 8-bit RGBA
        case 37: return alm::rhi::Format::RGBA8_UNORM;   // VK_FORMAT_R8G8B8A8_UNORM
        case 38: return alm::rhi::Format::RGBA8_SNORM;   // VK_FORMAT_R8G8B8A8_SNORM
        case 41: return alm::rhi::Format::RGBA8_UINT;    // VK_FORMAT_R8G8B8A8_UINT
        case 42: return alm::rhi::Format::RGBA8_SINT;    // VK_FORMAT_R8G8B8A8_SINT
        case 43: return alm::rhi::Format::SRGBA8_UNORM;  // VK_FORMAT_R8G8B8A8_SRGB
        case 44: return alm::rhi::Format::BGRA8_UNORM;   // VK_FORMAT_B8G8R8A8_UNORM
        case 48: return alm::rhi::Format::SBGRA8_UNORM;  // VK_FORMAT_B8G8R8A8_SRGB

        // 16-bit
        case 70: return alm::rhi::Format::R16_UNORM;     // VK_FORMAT_R16_UNORM
        case 71: return alm::rhi::Format::R16_SNORM;     // VK_FORMAT_R16_SNORM
        case 74: return alm::rhi::Format::R16_UINT;      // VK_FORMAT_R16_UINT
        case 75: return alm::rhi::Format::R16_SINT;      // VK_FORMAT_R16_SINT
        case 76: return alm::rhi::Format::R16_FLOAT;     // VK_FORMAT_R16_SFLOAT
        case 77: return alm::rhi::Format::RG16_UNORM;    // VK_FORMAT_R16G16_UNORM
        case 78: return alm::rhi::Format::RG16_SNORM;    // VK_FORMAT_R16G16_SNORM
        case 81: return alm::rhi::Format::RG16_UINT;     // VK_FORMAT_R16G16_UINT
        case 82: return alm::rhi::Format::RG16_SINT;     // VK_FORMAT_R16G16_SINT
        case 83: return alm::rhi::Format::RG16_FLOAT;    // VK_FORMAT_R16G16_SFLOAT

        // 16-bit RGBA
        case 91: return alm::rhi::Format::RGBA16_UNORM;  // VK_FORMAT_R16G16B16A16_UNORM
        case 92: return alm::rhi::Format::RGBA16_SNORM;  // VK_FORMAT_R16G16B16A16_SNORM
        case 95: return alm::rhi::Format::RGBA16_UINT;   // VK_FORMAT_R16G16B16A16_UINT
        case 96: return alm::rhi::Format::RGBA16_SINT;   // VK_FORMAT_R16G16B16A16_SINT
        case 97: return alm::rhi::Format::RGBA16_FLOAT;  // VK_FORMAT_R16G16B16A16_SFLOAT

        // 32-bit
        case 98:  return alm::rhi::Format::R32_UINT;     // VK_FORMAT_R32_UINT
        case 99:  return alm::rhi::Format::R32_SINT;     // VK_FORMAT_R32_SINT
        case 100: return alm::rhi::Format::R32_FLOAT;    // VK_FORMAT_R32_SFLOAT
        case 101: return alm::rhi::Format::RG32_UINT;    // VK_FORMAT_R32G32_UINT
        case 102: return alm::rhi::Format::RG32_SINT;    // VK_FORMAT_R32G32_SINT
        case 103: return alm::rhi::Format::RG32_FLOAT;   // VK_FORMAT_R32G32_SFLOAT
        case 104: return alm::rhi::Format::RGB32_UINT;   // VK_FORMAT_R32G32B32_UINT
        case 105: return alm::rhi::Format::RGB32_SINT;   // VK_FORMAT_R32G32B32_SINT
        case 106: return alm::rhi::Format::RGB32_FLOAT;  // VK_FORMAT_R32G32B32_SFLOAT
        case 107: return alm::rhi::Format::RGBA32_UINT;  // VK_FORMAT_R32G32B32A32_UINT
        case 108: return alm::rhi::Format::RGBA32_SINT;  // VK_FORMAT_R32G32B32A32_SINT
        case 109: return alm::rhi::Format::RGBA32_FLOAT; // VK_FORMAT_R32G32B32A32_SFLOAT

        // Packed
        case 64:  return alm::rhi::Format::R10G10B10A2_UNORM; // VK_FORMAT_A2B10G10R10_UNORM_PACK32
        case 122: return alm::rhi::Format::R11G11B10_FLOAT;   // VK_FORMAT_B10G11R11_UFLOAT_PACK32

        // Depth stencil
        case 124: return alm::rhi::Format::D16;          // VK_FORMAT_D16_UNORM
        case 126: return alm::rhi::Format::D32;          // VK_FORMAT_D32_SFLOAT
        case 129: return alm::rhi::Format::D24S8;        // VK_FORMAT_D24_UNORM_S8_UINT
        case 130: return alm::rhi::Format::D32S8;        // VK_FORMAT_D32_SFLOAT_S8_UINT

        // Block compressed
        case 131: return alm::rhi::Format::BC1_UNORM;        // VK_FORMAT_BC1_RGB_UNORM_BLOCK
        case 132: return alm::rhi::Format::BC1_UNORM_SRGB;   // VK_FORMAT_BC1_RGB_SRGB_BLOCK
        case 135: return alm::rhi::Format::BC2_UNORM;        // VK_FORMAT_BC2_UNORM_BLOCK
        case 136: return alm::rhi::Format::BC2_UNORM_SRGB;   // VK_FORMAT_BC2_SRGB_BLOCK
        case 137: return alm::rhi::Format::BC3_UNORM;        // VK_FORMAT_BC3_UNORM_BLOCK
        case 138: return alm::rhi::Format::BC3_UNORM_SRGB;   // VK_FORMAT_BC3_SRGB_BLOCK
        case 139: return alm::rhi::Format::BC4_UNORM;        // VK_FORMAT_BC4_UNORM_BLOCK
        case 140: return alm::rhi::Format::BC4_SNORM;        // VK_FORMAT_BC4_SNORM_BLOCK
        case 141: return alm::rhi::Format::BC5_UNORM;        // VK_FORMAT_BC5_UNORM_BLOCK
        case 142: return alm::rhi::Format::BC5_SNORM;        // VK_FORMAT_BC5_SNORM_BLOCK
        case 143: return alm::rhi::Format::BC6H_UFLOAT;      // VK_FORMAT_BC6H_UFLOAT_BLOCK
        case 144: return alm::rhi::Format::BC6H_SFLOAT;      // VK_FORMAT_BC6H_SFLOAT_BLOCK
        case 145: return alm::rhi::Format::BC7_UNORM;        // VK_FORMAT_BC7_UNORM_BLOCK
        case 146: return alm::rhi::Format::BC7_UNORM_SRGB;   // VK_FORMAT_BC7_SRGB_BLOCK

        // Without an equivalent in rhi::Format:
        // R8_SRGB(15), RG8_SRGB(22), BC1_RGBA(133,134), USCALED/SSCALED variants,
        // 48-bit RGB formats, ETC/ASTC/PVRTC...
        default:  return alm::rhi::Format::UNKNOWN;
        }
    }

    ktx_transcode_fmt_e SelectTargetFormat(ktxTexture2* tex, alm::gfx::TextureLoadFlags flags, alm::rhi::Device* device)
    {
        ktx_uint32_t numComponents = ktxTexture2_GetNumComponents(tex);
        bool hasAlpha = (numComponents == 4);

        if (alm::has_any_flag(flags, alm::gfx::TextureLoadFlags::IsNormalMap))
        {
            return device->SupportsTextureFormat(alm::rhi::Format::BC5_UNORM) ?
                KTX_TTF_BC5_RG : KTX_TTF_BC7_RGBA;
        }

        if (hasAlpha)
        {
            return device->SupportsTextureFormat(alm::rhi::Format::BC7_UNORM) ?
                KTX_TTF_BC7_RGBA : KTX_TTF_BC3_RGBA;
        }

        // 3. Sin alfa, preferir BC1 (m�s ligero) o BC7.
        return device->SupportsTextureFormat(alm::rhi::Format::BC1_UNORM) ?
            KTX_TTF_BC1_RGB : KTX_TTF_BC7_RGBA;
    }

    // Builds the full TextureData (fields, layout & data blob) taking ownership of the ScratchImage.
    // The blob points into the ScratchImage's own memory block, in D3D12 subresource order
    // (subresource = mip + arraySlice * mipLevels, as laid out by SetupImageArray)
    alm::gfx::TextureData TextureDataFromScratchImage(std::unique_ptr<DirectX::ScratchImage> image)
    {
        const DirectX::TexMetadata& metadata = image->GetMetadata();

        alm::gfx::TextureData texData;
        texData.format = alm::gfx::GetFormat(metadata.format);
        assert(texData.format != alm::rhi::Format::UNKNOWN);
        texData.Width = metadata.width;
        texData.Height = metadata.height;
        texData.MipLevels = metadata.mipLevels;
        texData.Depth = 1;
        texData.ArraySize = 1;

        switch (metadata.dimension)
        {
        case DirectX::TEX_DIMENSION_TEXTURE1D:
            texData.Dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::Texture1DArray : alm::rhi::TextureDimension::Texture1D;
            texData.ArraySize = metadata.arraySize;
            break;

        case DirectX::TEX_DIMENSION_TEXTURE2D:
            if (metadata.IsCubemap())
            {
                texData.Dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::TextureCubeArray : alm::rhi::TextureDimension::TextureCube;
                texData.ArraySize = metadata.arraySize * 6;
            }
            else
            {
                texData.Dimension = metadata.arraySize > 1 ? alm::rhi::TextureDimension::Texture2DArray : alm::rhi::TextureDimension::Texture2D;
                texData.ArraySize = metadata.arraySize;
            }
            break;

        case DirectX::TEX_DIMENSION_TEXTURE3D:
            assert(metadata.IsVolumemap());
            texData.Depth = metadata.depth;
            texData.Dimension = alm::rhi::TextureDimension::Texture3D;
            break;
        }

        {
            const uint8_t* base = image->GetPixels();

            texData.DataLayout.resize(texData.ArraySize);
            for (uint32_t arraySlice = 0; arraySlice < texData.ArraySize; ++arraySlice)
            {
                texData.DataLayout[arraySlice].resize(texData.MipLevels);
                for (uint32_t mip = 0; mip < texData.MipLevels; ++mip)
                {
                    const DirectX::Image* img = image->GetImage(mip, arraySlice, 0);
                    assert(img);

                    auto& level = texData.DataLayout[arraySlice][mip];
                    level.RowPitch = img->rowPitch;
                    level.DepthPitch = img->slicePitch;
                    level.DataOffset = img->pixels - base;

                    const size_t depthSlices = std::max(texData.Depth >> mip, 1u);
                    level.DataSize = img->slicePitch * depthSlices;

                    assert(level.DataOffset >= 0);
                    assert(level.DataOffset + level.DataSize <= image->GetPixelsSize());
                }
            }
        }

        DirectX::ScratchImage* image_ptr = image.release();
        texData.Data = alm::Blob{
            image_ptr->GetPixels(),
            image_ptr->GetPixelsSize(),
            (uint8_t*)image_ptr,
            [](void* ptr) { delete (DirectX::ScratchImage*)ptr; } };

        return texData;
    }

    // Builds the full TextureData (fields, layout & data blob) taking ownership of the ktxTexture.
    // The format is resolved by the caller: it needs flags/device context (transcoded files keep
    // VK_FORMAT_UNDEFINED, libktx does not update vkFormat after ktxTexture2_TranscodeBasis).
    // DataLayout offsets are the real file offsets: KTX2 stores levels mip-major (level_base first,
    // each level with all layers & faces concatenated in layer, face, slice order)
    alm::gfx::TextureData TextureDataFromKtxTexture(ktxTexture* tex, alm::rhi::Format format)
    {
        alm::gfx::TextureData texData{};
        texData.format = format;
        texData.Width = tex->baseWidth;
        texData.Height = tex->baseHeight;
        texData.Depth = tex->baseDepth;
        texData.MipLevels = tex->numLevels;
        texData.ArraySize = tex->numLayers;

        if (tex->numFaces == 6)
        {
            texData.Dimension = tex->numLayers > 1 ? alm::rhi::TextureDimension::TextureCubeArray : alm::rhi::TextureDimension::TextureCube;
            texData.ArraySize = tex->numLayers * 6;
        }
        else if (tex->baseDepth > 1)
        {
            texData.Dimension = alm::rhi::TextureDimension::Texture3D;
        }
        else
        {
            texData.Dimension = tex->numLayers > 1 ? alm::rhi::TextureDimension::Texture2DArray : alm::rhi::TextureDimension::Texture2D;
        }

        const size_t dataSize = ktxTexture_GetDataSize(tex);

        texData.DataLayout.resize(texData.ArraySize);
        for (uint32_t arraySlice = 0; arraySlice < texData.ArraySize; ++arraySlice)
        {
            const uint32_t layer = arraySlice / tex->numFaces;
            const uint32_t face = arraySlice % tex->numFaces;

            texData.DataLayout[arraySlice].resize(texData.MipLevels);
            for (uint32_t mip = 0; mip < texData.MipLevels; ++mip)
            {
                ktx_size_t offset = 0;
                KTX_error_code res = ktxTexture_GetImageOffset(tex, mip, layer, face, &offset);
                assert(res == KTX_SUCCESS);

                const size_t imageSize = ktxTexture_GetImageSize(tex, mip);
                const size_t depthSlices = (texData.Dimension == alm::rhi::TextureDimension::Texture3D)
                    ? std::max(texData.Depth >> mip, 1u)
                    : 1u;

                auto& level = texData.DataLayout[arraySlice][mip];
                level.DataOffset = offset;
                level.RowPitch = ktxTexture_GetRowPitch(tex, mip);
                level.DepthPitch = imageSize;
                level.DataSize = imageSize * depthSlices;

                assert(level.DataOffset + level.DataSize <= (ptrdiff_t)dataSize);
            }
        }

        texData.Data = alm::Blob{
            ktxTexture_GetData(tex),
            dataSize,
            (uint8_t*)tex,
            [](void* ptr) { ktxTexture_Destroy((ktxTexture*)ptr); } };

        return texData;
    }
} // anonymous namespace

std::expected<alm::gfx::TextureData, std::string>
alm::gfx::LoadDDSTexture(const alm::WeakBlob& fileData)
{
	DirectX::TexMetadata metadata;
    auto image = std::make_unique<DirectX::ScratchImage>();
	HRESULT hr = DirectX::LoadFromDDSMemory((std::byte*)fileData.data(), fileData.size(), DirectX::DDS_FLAGS_NONE, &metadata, *image);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("Error loading DDS image, hr [{:#x}]", hr));
	}

    return TextureDataFromScratchImage(std::move(image));
}

std::expected<alm::gfx::TextureData, std::string>
alm::gfx::LoadKTXTexture(const alm::WeakBlob& fileData, TextureLoadFlags flags, rhi::Device* device)
{
    ktxTexture* ktxTex = nullptr;

    // Create texture from memory
    KTX_error_code result = ktxTexture_CreateFromMemory(
        static_cast<const ktx_uint8_t*>(fileData.data()),
        fileData.size(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &ktxTex);

    if (result != KTX_SUCCESS)
    {
        return std::unexpected(std::format("Error creating ktxTexture: {}", ktxErrorString(result)));
    }

    if (ktxTex->classId != ktxTexture2_c)
    {
        ktxTexture_Destroy(ktxTex);
        return std::unexpected("KTX1 textures are not supported, use KTX2.");
    }
    ktxTexture2* ktxTex2 = reinterpret_cast<ktxTexture2*>(ktxTex);

    // Chech if the texture needs transcoding
    rhi::Format rhiFmt = rhi::Format::UNKNOWN;
    if (ktxTexture2_NeedsTranscoding(ktxTex2))
    {
        // Transcode to a GPU native format
        ktx_transcode_fmt_e targetFormat = SelectTargetFormat(ktxTex2, flags, device);                
        result = ktxTexture2_TranscodeBasis(ktxTex2, targetFormat, 0);
        if (result != KTX_SUCCESS)
        {
            ktxTexture_Destroy(ktxTex);
            return std::unexpected(std::format("Error al transcodificar KTX2: {}", ktxErrorString(result)));
        }
        rhiFmt = MapTranscodeFormatToRHI(targetFormat);
    }
    else
    {
        rhiFmt = MapVkFormatToRHI(ktxTex2->vkFormat);
        if (rhiFmt == rhi::Format::UNKNOWN)
        {
            ktxTexture_Destroy(ktxTex);
            return std::unexpected(std::format("Unsupported vkFormat ({}) in KTX2 file.", ktxTex2->vkFormat));
        }
    }

    return TextureDataFromKtxTexture(ktxTex, rhiFmt);
}

std::expected<alm::gfx::TextureData, std::string>
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

    alm::gfx::TextureData texData;
	//texData.originalBitsPerPixel = static_cast<uint32_t>(originalChannels) * (is_hdr ? 32 : 8);
    texData.Width = static_cast<uint32_t>(width);
    texData.Height = static_cast<uint32_t>(height);
    texData.MipLevels = 1;
    texData.Dimension = alm::rhi::TextureDimension::Texture2D;

    texData.DataLayout.resize(1);
    texData.DataLayout[0].resize(1);
    texData.DataLayout[0][0].DataOffset = 0;
    texData.DataLayout[0][0].RowPitch = static_cast<size_t>(width * bytesPerPixel);
    texData.DataLayout[0][0].DataSize = static_cast<size_t>(width * height * bytesPerPixel);

    texData.Data = alm::Blob{ 
		bitmap, (size_t)(width * height * bytesPerPixel), bitmap, [](void* ptr) { stbi_image_free(ptr); } };

	switch (channels)
	{
	case 1:
        texData.format = is_hdr ? alm::rhi::Format::R32_FLOAT : alm::rhi::Format::R8_UNORM;
		break;
	case 2:
        texData.format = is_hdr ? alm::rhi::Format::RG32_FLOAT : alm::rhi::Format::RG8_UNORM;
		break;
	case 4:
        texData.format = is_hdr ? alm::rhi::Format::RGBA32_FLOAT :
            forceSRGB ? alm::rhi::Format::SRGBA8_UNORM : alm::rhi::Format::RGBA8_UNORM;
		break;
	default:
		return std::unexpected(std::format("Unsupported number of components ({})", channels));
	}

    return texData;
}