#include "Gfx/TextureCache.h"
#include <nvrhi/d3d12.h>
#include "Core/Log.h"
#include "Core/File.h"
#include "Gfx/DDSLoader.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include <filesystem>
#include <DirectXTex.h>


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

st::gfx::TextureCache::TextureCache(st::gfx::DeviceManager* deviceManager) : 
    m_DeviceManager{ deviceManager }
{}

std::expected<std::shared_ptr<st::gfx::TextureCache::Handle>, std::string>
st::gfx::TextureCache::Load(const std::string& path)
{
    st::fs::File file{ path };
    if (!file.IsOpen())
    {
        LOG_WARNING("file {} not found.", path);
        return std::unexpected(std::format("file {} not found.", path));
    }
    
    auto readResult = file.Read();
    assert(readResult);
    st::Blob fileData = std::move(*readResult);

    std::filesystem::path spath{ path };
    const std::string ext = spath.extension().string();
    if (ext == ".dds" || ext == ".DDS")
    {
        auto ddsResult = LoadDDSTexture(path);
        if (!ddsResult)
        {
            return std::unexpected(std::move(ddsResult.error()));
        }

        auto texResult = CreateTextureFromDDSMetadata((*ddsResult)->GetMetadata(), path, m_DeviceManager->GetDevice());
        if (!texResult)
        {
            return std::unexpected(std::move(texResult.error()));
        }

        auto uploadResult = m_DeviceManager->GetDataUploader()->UploadTextureData(
            st::WeakBlob{ (char*)(*ddsResult)->GetPixels(), (*ddsResult)->GetPixelsSize() },
            *texResult, nvrhi::ResourceStates::Common, nvrhi::ResourceStates::ShaderResource, nvrhi::AllSubresources, path.c_str());
        if (!uploadResult)
        {
            return std::unexpected(std::move(texResult.error()));
        }

        std::shared_ptr<Handle> handle{ new Handle, [this](Handle* h) {
            std::scoped_lock lock{ m_StaleMapMutex };
            m_StaleTextures.push_back(std::move(h->path));
        }};

        handle->texture = *texResult;
        handle->path = path;
        handle->state = Handle::Loading;

        {
            std::scoped_lock loc{ m_MapMutex };
            m_Textures.emplace(path, std::weak_ptr<Handle>(handle));
        }
        {
            std::scoped_lock loc{ m_InFlightMapMutex };
            m_InFlightTextures.emplace(path, std::make_pair(handle, *uploadResult));
        }
        
        return handle;
    }
    else if (ext == ".png" || ext == ".PNG")
    {
    }

	return {};
}

void st::gfx::TextureCache::Update()
{
    std::scoped_lock lockMaps{ m_MapMutex };
    std::scoped_lock lockStale{ m_StaleMapMutex };

    for (const std::string& path : m_StaleTextures)
    {
        // Should exists in textures map
        auto tex_it = m_Textures.find(path);
        assert(tex_it != m_Textures.end());
        m_Textures.erase(tex_it);
    }

    m_StaleTextures.clear();
}