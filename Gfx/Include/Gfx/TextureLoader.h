#pragma once

#include "Core/Blob.h"
#include "Gfx/TextureInfo.h"
#include "Gfx/TextureLoadFlags.h"
#include "RHI/ResourceState.h"

namespace alm::gfx
{

std::expected<std::pair<TextureInfo, alm::Blob>, std::string> LoadDDSTexture(const alm::WeakBlob& fileData);
std::expected<std::pair<TextureInfo, alm::Blob>, std::string> LoadKTXTexture(const alm::WeakBlob& fileData,
	TextureLoadFlags flags, rhi::Device* device);
std::expected<std::pair<TextureInfo, alm::Blob>, std::string> LoadImageTexture(const alm::WeakBlob& fileData,
	bool forceSRGB);

bool SaveDDSTexture(rhi::TextureHandle texture, rhi::ResourceState currentState, rhi::ResourceState targetState, rhi::Device* device,
	const std::string& filename);

} // namespace st::gfx