#pragma once

#include <expected>
#include <memory>
#include "Gfx/TextureInfo.h"
#include "Core/Blob.h"

namespace st::gfx
{

std::expected<std::pair<TextureInfo, st::Blob>, std::string> LoadDDSTexture(const st::WeakBlob& fileData);
std::expected<std::pair<TextureInfo, st::Blob>, std::string> LoadImageTexture(const st::WeakBlob& fileData, bool forceSRGB);

} // namespace st::gfx