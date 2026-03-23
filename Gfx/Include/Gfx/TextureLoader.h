#pragma once

#include <expected>
#include <memory>
#include "Gfx/TextureInfo.h"
#include "Core/Blob.h"

namespace alm::gfx
{

std::expected<std::pair<TextureInfo, alm::Blob>, std::string> LoadDDSTexture(const alm::WeakBlob& fileData);
std::expected<std::pair<TextureInfo, alm::Blob>, std::string> LoadImageTexture(const alm::WeakBlob& fileData, bool forceSRGB);

} // namespace st::gfx