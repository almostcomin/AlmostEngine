#include "Gfx/TextureCache.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "Gfx/DDSLoader.h"
#include <filesystem>

st::gfx::TextureCache::TextureCache(nvrhi::DeviceHandle device) : m_Device{ device }
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
        LoadDDSTexture(path, m_Device);
    }
    else if (ext == ".png" || ext == ".PNG")
    {
    }

	return {};
}