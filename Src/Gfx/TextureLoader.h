#pragma once

#include <nvrhi/nvrhi.h>
#include <expected>
#include <memory>
#include "Gfx/TextureInfo.h"
#include "Core/Blob.h"

namespace DirectX
{
	class ScratchImage;
}

namespace st::gfx
{

std::expected<std::pair<TextureInfo, std::unique_ptr<DirectX::ScratchImage>>, std::string> LoadDDSTexture(const std::string& path);
std::expected<std::pair<st::gfx::TextureInfo, st::Blob>, std::string> LoadImageTexture(const std::string& path);

} // namespace st::gfx