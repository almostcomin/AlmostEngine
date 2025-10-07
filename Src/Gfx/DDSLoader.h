#pragma once

#include <nvrhi/nvrhi.h>
#include <expected>
#include <memory>

namespace DirectX
{
	class ScratchImage;
}

namespace st::gfx
{

std::expected<std::unique_ptr<DirectX::ScratchImage>, std::string> LoadDDSTexture(const std::string& path);

} // namespace st::gfx