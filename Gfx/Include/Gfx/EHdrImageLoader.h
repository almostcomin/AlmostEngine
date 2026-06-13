#pragma once

#include "Gfx/TextureInfo.h"
#include "Core/Blob.h"

namespace alm::gfx
{
	std::expected<std::pair<alm::gfx::TextureInfo, alm::Blob>, std::string>
	LoadEHdr(const std::string& hdrPath);
}