#include "Gfx/DDSLoader.h"
#include "Core/File.h"
#include "Core/Log.h"

#include <DirectXTex.h>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

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

std::expected<std::pair<st::gfx::TextureInfo, st::Blob>, std::string> LoadImageTexture(const std::string& path)
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
	texInfo.isRenderTarget = true;
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
		texInfo.format = is_hdr ? nvrhi::Format::RGBA32_FLOAT :
			(texInfo.forceSRGB ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM);
		break;
	default:
		return std::unexpected(std::format("Unsupported number of components ({}) for texture '{}'", channels, path));
	}

	return std::pair<st::gfx::TextureInfo, st::Blob>{ texInfo, std::move(blob) };
}