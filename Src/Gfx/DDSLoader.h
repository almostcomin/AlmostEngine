#pragma once

#include <nvrhi/nvrhi.h>
#include <expected>

namespace st::gfx
{

std::expected<nvrhi::TextureHandle, std::string> LoadDDSTexture(const std::string& path, nvrhi::DeviceHandle device);

} // namespace st::gfx