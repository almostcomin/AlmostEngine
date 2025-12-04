#include "Gfx/TextureCache.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "Gfx/TextureLoader.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/LoadedTexture.h"
#include "RenderAPI/Device.h"
#include <filesystem>

namespace
{

st::rapi::TextureDesc TexInfoToTexDesc(const st::gfx::TextureInfo& texInfo)
{
    return st::rapi::TextureDesc {
        .width = texInfo.width,
        .height = texInfo.height,
        .depth = texInfo.depth,
        .arraySize = texInfo.arraySize,
        .mipLevels = texInfo.mipLevels,
        .sampleCount = 1,
        .sampleQuality = 0,
        .format = texInfo.format,
        .dimension = texInfo.dimension,
        .debugName = texInfo.debugName,
        .shaderUsage = st::rapi::ShaderUsage::ShaderResource
    };
}

st::rapi::TextureHandle CreateTextureFromTexInfo(const st::gfx::TextureInfo& texInfo, st::rapi::Device* device)
{
    st::rapi::TextureDesc desc = TexInfoToTexDesc(texInfo);
    return device->CreateTexture(desc, st::rapi::ResourceState::COPY_DST);
}

} // anonymous namespace

st::gfx::TextureCache::TextureCache(rapi::Device* device, st::gfx::DataUploader* dataUploader) :
    m_Device{ device }, m_DataUploader{ dataUploader }
{}

std::shared_ptr<st::gfx::LoadedTexture> st::gfx::TextureCache::Get(const std::string& id)
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

st::gfx::TextureCache::LoadResult st::gfx::TextureCache::Load(const std::string& path, bool forceSRGB)
{
    auto textureHandle = Get(path);
    if (textureHandle)
    {
        return std::pair<std::shared_ptr<st::gfx::LoadedTexture>, st::SignalListener>(textureHandle, {});
    }

    st::fs::File file{ path };
    if (!file.IsOpen())
    {
        LOG_WARNING("file {} not found.", path);
        return std::unexpected(std::format("file {} not found.", path));
    }
    
    auto readResult = file.Read();
    assert(readResult);
    st::Blob fileData = std::move(*readResult);

    std::string_view ext = GetExtensionFromPath(path);
    auto loadResult = LoadInternal(st::WeakBlob{ fileData }, path, (ext == "dds" || ext == "DDS"), forceSRGB);
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }
    
    return loadResult;
}

st::gfx::TextureCache::LoadResult st::gfx::TextureCache::Load(const st::WeakBlob& blob, const std::string& id, bool isDDS, bool forceSRGB)
{
    auto textureHandle = Get(id);
    if (textureHandle)
    {
        return std::pair<std::shared_ptr<st::gfx::LoadedTexture>, st::SignalListener>(textureHandle, {});
    }

    auto loadResult = LoadInternal(blob, id, isDDS, forceSRGB);
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }

    return loadResult;
}

void st::gfx::TextureCache::Update()
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

    // Check stale textures
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
}

st::gfx::TextureCache::LoadResult st::gfx::TextureCache::LoadInternal(const st::WeakBlob& blob, const std::string& id, bool isDDS, bool forceSRGB)
{
    std::expected<std::pair<st::gfx::TextureInfo, st::Blob>, std::string> loadResult;
    if (isDDS)
    {
        loadResult = LoadDDSTexture(blob);
    }
    else
    {
        loadResult = LoadImageTexture(blob, forceSRGB);
    }
    if (!loadResult)
    {
        return std::unexpected(std::move(loadResult.error()));
    }

    TextureInfo& texInfo = loadResult->first;
    texInfo.debugName = id;
    rapi::TextureHandle texture = CreateTextureFromTexInfo(texInfo, m_Device);
    if (!texture)
    {
        return std::unexpected(std::format("Failed creating texture {}.", id));
    }

    auto uploadResult = m_DataUploader->UploadTextureData(
        WeakBlob{ loadResult->second }, texture, rapi::ResourceState::COPY_DST, rapi::ResourceState::SHADER_RESOURCE, rapi::AllSubresources, texInfo.debugName.c_str());
    if (!uploadResult)
    {
        return std::unexpected(std::move(uploadResult.error()));
    }

    std::shared_ptr<LoadedTexture> handle;
    handle = CreateHandle();
    handle->texture = texture;
    handle->id = id;
    handle->state = LoadedTexture::Loading;

    st::SignalListener uploadEvent = std::move(*uploadResult);

    {
        std::scoped_lock loc{ m_MapMutex };
        m_Textures.emplace(id, std::weak_ptr<LoadedTexture>(handle));
    }
    {
        std::scoped_lock loc{ m_InFlightMapMutex };
        m_InFlightTextures.push_back(InFlightData{ id, handle, uploadEvent });
    }

    return std::pair<std::shared_ptr<st::gfx::LoadedTexture>, st::SignalListener>
        { std::move(handle), uploadEvent };
}

std::shared_ptr<st::gfx::LoadedTexture> st::gfx::TextureCache::CreateHandle()
{
    return std::shared_ptr<LoadedTexture>{ new LoadedTexture, [this](LoadedTexture* h) {
        std::scoped_lock lock{ m_StaleMapMutex };
        m_StaleTextures.push_back(std::move(h->id));
    } };
}