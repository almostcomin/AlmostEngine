#pragma once

#include "Gfx/TextureInfo.h"
#include "Core/Blob.h"

namespace alm::gfx
{
    struct EHdrInfo
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        float CellSize = 1.f;       // world units per pixel (meters if CRS is UTM)
        bool HasCellSize = false;
    };

	std::expected<std::pair<alm::gfx::EHdrInfo, alm::Blob>, std::string>
	LoadEHdr(const std::string& hdrPath);
}