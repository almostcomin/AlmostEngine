#include "Gfx/DDSLoader.h"
#include "Core/File.h"
#include "Core/Log.h"

#include <DirectXTex.h>
#include <filesystem>

namespace
{


} // anonymous namespace

std::expected<std::unique_ptr<DirectX::ScratchImage>, std::string> st::gfx::LoadDDSTexture(const std::string& path)
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

    auto image = std::make_unique<DirectX::ScratchImage>();
	HRESULT hr = DirectX::LoadFromDDSMemory((std::byte*)fileData.data(), fileData.size(), DirectX::DDS_FLAGS_NONE, nullptr, *image);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("Error loading DDS image, file '{}', hr [{:#x}]", path, hr));
	}
    assert(image->GetMetadata().dimension == DirectX::TEX_DIMENSION_TEXTURE2D); // TODO

    return image;
}