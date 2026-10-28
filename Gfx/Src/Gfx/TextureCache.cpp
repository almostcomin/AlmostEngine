#include "Gfx/GfxPCH.h"
#include "Gfx/TextureCache.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "Gfx/TextureLoader.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/LoadedTexture.h"
#include "RHI/Device.h"
#include <filesystem>

namespace
{

alm::rhi::TextureDesc TexDataToTexDesc(const alm::gfx::TextureData& texData)
{
    return alm::rhi::TextureDesc {
        .width = texData.Width,
        .height = texData.Height,
        .depth = texData.Depth,
        .arraySize = texData.ArraySize,
        .mipLevels = texData.MipLevels,
        .sampleCount = 1,
        .sampleQuality = 0,
        .format = texData.format,
        .dimension = texData.Dimension,
        .shaderUsage = alm::rhi::TextureShaderUsage::Sampled
    };
}

// Resturns [texture, originalMips]
// The texture is created in COPY_DST state
std::pair<alm::rhi::TextureOwner, uint32_t> CreateTextureFromTexData(const alm::gfx::TextureData& texData, bool genMips, alm::rhi::Device* device)
{
    alm::rhi::TextureDesc desc = TexDataToTexDesc(texData);
    uint32_t originalMips = desc.mipLevels;
    if (genMips)
    {
        uint32_t maxDim = std::max(desc.width, desc.height);
        desc.mipLevels = std::bit_width(maxDim);
        if (desc.mipLevels != originalMips)
        {
            desc.shaderUsage |= alm::rhi::TextureShaderUsage::Storage; // Needed for mip generation
        }
    }
    return { device->CreateTexture(desc, alm::rhi::ResourceState::COPY_DST, texData.DebugName), originalMips };
}

} // anonymous namespace

alm::gfx::TextureCache::TextureCache(alm::gfx::DataUploader* dataUploader, rhi::Device* device) :
    m_Device{ device }, m_DataUploader{ dataUploader }
{}

alm::gfx::TextureCache::~TextureCache()
{
    RemoveStaleTextures();

    for (const auto& [id, weakTex] : m_Textures)
    {
        if (!weakTex.expired())
        {
            LOG_ERROR("Texture '{}' leaked: still alive when TextureCache was destroyed.", id);
        }
    }

    assert(m_StaleTextures.empty());
    assert(m_InFlightTextures.empty());
}

std::shared_ptr<alm::gfx::LoadedTexture> alm::gfx::TextureCache::Get(const std::string& id)
{
    std::scoped_lock lockMaps{ m_MapMutex };
    auto it = m_Textures.find(id);
    if (it != m_Textures.end() && !it->second.expired())
    {
        return it->second.lock();
    }
    else
    {
        return {};
    }
}

alm::gfx::TextureCache::LoadResult alm::gfx::TextureCache::Load(const std::string& path, TextureLoadFlags flags)
{
    auto textureHandle = Get(path);
    if (textureHandle)
    {
        return std::pair<std::shared_ptr<alm::gfx::LoadedTexture>, alm::SignalListener>(textureHandle, {});
    }

    alm::fs::File file{ path };
    if (!file.IsOpen())
    {
        LOG_WARNING("file {} not found.", path);
        return std::unexpected(std::format("file {} not found.", path));
    }
    
    auto readResult = file.Read();
    assert(readResult);
    alm::Blob fileData = std::move(*readResult);

    auto loadResult = LoadInternal(alm::WeakBlob{ fileData }, flags, GetFileFormatFromExtension(GetExtensionFromPath(path)), path);
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }
    
    return loadResult;
}

alm::gfx::TextureCache::LoadResult alm::gfx::TextureCache::Load(const alm::WeakBlob& blob, FileFormat format, TextureLoadFlags flags,
    const std::string& id)
{
    auto textureHandle = Get(id);
    if (textureHandle)
    {
        return std::pair<std::shared_ptr<alm::gfx::LoadedTexture>, alm::SignalListener>(textureHandle, {});
    }

    auto loadResult = LoadInternal(blob, flags, format, id);
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }

    return loadResult;
}

void alm::gfx::TextureCache::Update()
{
    // Check finished loaded textures
    {
        std::scoped_lock lockInFlight{ m_InFlightMapMutex };
        std::scoped_lock lockMaps{ m_MapMutex };

        for (int i = 0; i < m_InFlightTextures.size();)
        {
            if(m_InFlightTextures[i].event.Poll())
            {
                m_Textures[m_InFlightTextures[i].id].lock()->state = LoadedTexture::State::Ready;
                m_InFlightTextures.erase(m_InFlightTextures.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    }

    RemoveStaleTextures();
}

alm::gfx::TextureCache::FileFormat alm::gfx::TextureCache::GetFileFormatFromExtension(std::string_view ext)
{
    auto format = FileFormat::Other;
    if (ext == "dds" || ext == "DDS")
    {
        format = FileFormat::DDS;
    }
    else if (ext == "ktx2" || ext == "KTX2")
    {
        format = FileFormat::KTX;
    }

    return format;
}

alm::gfx::TextureCache::LoadResult alm::gfx::TextureCache::LoadInternal(
    const alm::WeakBlob& blob, alm::gfx::TextureLoadFlags flags, FileFormat format, const std::string& id)
{
    alm::gfx::TextureLoadFlags tflags = flags;
    std::expected<alm::gfx::TextureData, std::string> loadResult;
    switch (format)
    {
    case FileFormat::DDS:
        loadResult = LoadDDSTexture(blob);
        break;
    case FileFormat::KTX:
        loadResult = LoadKTXTexture(blob, tflags, m_Device);
        break;
    case FileFormat::Other:
        loadResult = LoadImageTexture(blob, has_any_flag(tflags, alm::gfx::TextureLoadFlags::ForceSRGB));
        break;
    }
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }

    TextureData& texData = *loadResult;
    texData.DebugName = id;

    if (!m_Device->SupportsTextureFormat(texData.format))
    {
        return std::unexpected(std::format(
            "Texture '{}' uses format {} which is not supported by the device.",
            id, rhi::GetFormatString(texData.format)));
    }

    auto [texture, originalMips] = CreateTextureFromTexData(texData, has_any_flag(flags, TextureLoadFlags::GenerateMips), m_Device);
    if (!texture)
    {
        return std::unexpected(std::format("Failed creating texture {}.", id));
    }

    auto getMipsMethod = DataUploader::GenMipsMethod::None;
    if (has_any_flag(flags, TextureLoadFlags::GenerateMips) && (originalMips != texture->GetDesc().mipLevels))
    {
        getMipsMethod = has_any_flag(flags, TextureLoadFlags::IsNormalMap) ?
            DataUploader::GenMipsMethod::GenMips_NormalMap : DataUploader::GenMipsMethod::GenMips_Color;
    }

    auto uploadResult = m_DataUploader->UploadTextureData(
        *loadResult,
        texture.get_weak(),
        rhi::ResourceState::COPY_DST,
        rhi::ResourceState::SHADER_RESOURCE,        
        rhi::TextureSubresourceSet{ 0, texData.MipLevels, 0, texData.ArraySize },
        getMipsMethod,
        texData.DebugName.c_str());
    if (!uploadResult)
    {
        return std::unexpected(std::move(uploadResult.error()));
    }

    std::shared_ptr<LoadedTexture> handle;
    handle = CreateHandle();
    handle->texture = std::move(texture);
    handle->id = id;
    handle->state = LoadedTexture::Loading;

    alm::SignalListener uploadEvent = std::move(*uploadResult);

    {
        std::scoped_lock lock{ m_MapMutex };
        m_Textures.emplace(id, std::weak_ptr<LoadedTexture>(handle));
    }
    {
        std::scoped_lock lock{ m_InFlightMapMutex };
        m_InFlightTextures.push_back(InFlightData{ id, handle, uploadEvent });
    }

    return std::pair<std::shared_ptr<alm::gfx::LoadedTexture>, alm::SignalListener>
        { std::move(handle), uploadEvent };
}

std::shared_ptr<alm::gfx::LoadedTexture> alm::gfx::TextureCache::CreateHandle()
{
    return std::shared_ptr<LoadedTexture>{ new LoadedTexture, [this](LoadedTexture* h) {
        std::scoped_lock lock{ m_StaleMapMutex };
        m_StaleTextures.push_back(std::move(h->id));
        delete h;
    } };
}

void alm::gfx::TextureCache::RemoveStaleTextures()
{
    std::scoped_lock lockStale{ m_StaleMapMutex };
    std::scoped_lock lockMaps{ m_MapMutex };

    for (const std::string& path : m_StaleTextures)
    {
        // Should exists in textures map
        auto tex_it = m_Textures.find(path);
        assert(tex_it != m_Textures.end());
        m_Textures.erase(tex_it);
    }
    m_StaleTextures.clear();
}