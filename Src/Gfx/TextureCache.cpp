#include "Gfx/TextureCache.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "Gfx/TextureLoader.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include <filesystem>
#include <DirectXTex.h>

namespace
{
    nvrhi::TextureDesc TexInfoToNvrhiTexDesc(const st::gfx::TextureInfo& texInfo)
    {
        return nvrhi::TextureDesc {
            .width = texInfo.width,
            .height = texInfo.height,
            .depth = texInfo.depth,
            .arraySize = texInfo.arraySize,
            .mipLevels = texInfo.mipLevels,
            .sampleCount = 1,
            .sampleQuality = 0,
            .format = texInfo.format,
            .dimension = texInfo.dimension,
            .debugName = texInfo.debugName
        };
    }

    nvrhi::TextureHandle CreateTextureFromTexInfo(const st::gfx::TextureInfo& texInfo, nvrhi::DeviceHandle device)
    {
        nvrhi::TextureDesc desc = TexInfoToNvrhiTexDesc(texInfo);
        return device->createTexture(desc);
    };
}

st::gfx::TextureCache::TextureCache(nvrhi::DeviceHandle device, st::gfx::DataUploader* dataUploader) :
    m_Device{ device }, m_DataUploader{ dataUploader }
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

    std::shared_ptr<Handle> handle;
    nvrhi::EventQueryHandle uploadEvent;

    std::string_view ext = GetExtensionFromPath(path);
    if (ext == "dds" || ext == "DDS")
    {
        auto ddsResult = LoadDDSTexture(path);
        if (!ddsResult)
        {
            return std::unexpected(std::move(ddsResult.error()));
        }
        st::gfx::TextureInfo& texInfo = ddsResult->first;
        texInfo.debugName = st::GetFilenameFromPath(path);

        nvrhi::TextureHandle texture = CreateTextureFromTexInfo(texInfo, m_Device);
        if (!texture)
        {
            return std::unexpected(std::format("Failed creating texture {}.", path));
        }

        DirectX::ScratchImage* image = ddsResult->second.get();
        auto uploadResult = m_DataUploader->UploadTextureData(
            st::WeakBlob{ (char*)image->GetPixels(), image->GetPixelsSize() },
            texture, nvrhi::ResourceStates::Common, nvrhi::ResourceStates::ShaderResource, nvrhi::AllSubresources, texInfo.debugName.c_str());
        if (!uploadResult)
        {
            return std::unexpected(std::move(uploadResult.error()));
        }
        uploadEvent = *uploadResult;

        handle = CreateHandle();
        handle->texture = texture;
        handle->path = path;
        handle->state = Handle::Loading;
    }

    else if (ext == ".png" || ext == ".PNG")
    {
        auto stbResult = LoadImageTexture(path);
        if (!stbResult)
        {
            return std::unexpected(std::move(stbResult.error()));
        }
        st::gfx::TextureInfo& texInfo = stbResult->first;
        texInfo.debugName = st::GetFilenameFromPath(path);

        nvrhi::TextureHandle texture = CreateTextureFromTexInfo(texInfo, m_Device);
        if (!texture)
        {
            return std::unexpected(std::format("Failed creating texture {}.", path));
        }

        st::Blob& blob = stbResult->second;
        auto uploadResult = m_DataUploader->UploadTextureData(
            std::move(blob), texture, nvrhi::ResourceStates::Common, nvrhi::ResourceStates::ShaderResource, nvrhi::AllSubresources, texInfo.debugName.c_str());
        if (!uploadResult)
        {
            return std::unexpected(std::move(uploadResult.error()));
        }
        uploadEvent = *uploadResult;

        handle = CreateHandle();
        handle->texture = texture;
        handle->path = path;
        handle->state = Handle::Loading;
    }

    if (handle && uploadEvent)
    {
        {
            std::scoped_lock loc{ m_MapMutex };
            m_Textures.emplace(path, std::weak_ptr<Handle>(handle));
        }
        {
            std::scoped_lock loc{ m_InFlightMapMutex };
            m_InFlightTextures.emplace(path, std::make_pair(handle, uploadEvent));
        }

        return handle;
    }

    return std::unexpected(std::format("Error Loading texture {}", path));
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

std::shared_ptr<st::gfx::TextureCache::Handle> st::gfx::TextureCache::CreateHandle()
{
    return std::shared_ptr<Handle>{ new Handle, [this](Handle* h) {
        std::scoped_lock lock{ m_StaleMapMutex };
        m_StaleTextures.push_back(std::move(h->path));
    } };
}