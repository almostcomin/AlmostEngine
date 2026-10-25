#pragma once

#include "Core/Blob.h"
#include "Gfx/TextureData.h"
#include "Gfx/TextureLoadFlags.h"
#include "RHI/ResourceState.h"

namespace alm::gfx
{

std::expected<TextureData, std::string> LoadDDSTexture(const alm::WeakBlob& fileData);

std::expected<TextureData, std::string> LoadKTXTexture(const alm::WeakBlob& fileData,
	TextureLoadFlags flags, rhi::Device* device);

std::expected<TextureData, std::string> LoadImageTexture(const alm::WeakBlob& fileData,
	bool forceSRGB);

bool SaveDDSTexture(rhi::TextureHandle texture, rhi::ResourceState currentState, rhi::ResourceState targetState, rhi::Device* device,
	const std::string& filename);

} // namespace st::gfx