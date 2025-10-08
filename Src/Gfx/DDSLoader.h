#pragma once

#include <nvrhi/nvrhi.h>
#include <expected>
#include <memory>
#include "Gfx/TextureData.h"
#include "Core/Blob.h"

namespace DirectX
{
	class ScratchImage;
}

namespace st::gfx
{

std::expected<std::unique_ptr<DirectX::ScratchImage>, std::string> LoadDDSTexture(const std::string& path);
std::expected<std::pair<TextureInfo, Blob>, std::string> LoadImageTexture(const std::string& path);

} // namespace st::gfx