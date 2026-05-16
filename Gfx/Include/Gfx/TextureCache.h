#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <mutex>
#include "Core/Blob.h"
#include "Core/Common.h"
#include "Core/Signal.h"

namespace alm::rhi
{
	class Device;
}

namespace alm::gfx
{
	class DataUploader;
	class LoadedTexture;
}

namespace alm::gfx
{

class TextureCache
{
public:

	enum Flags
	{
		None = 0,
		ForceSRGB = 1 << 0,			// SRGB format
		GenerateMips = 1 << 1,	// Generate mips if not present in the data
		IsNormalMap = 1 << 2	// When generate mips, renormalize
	};

	using LoadResult = std::expected<std::pair<std::shared_ptr<LoadedTexture>, alm::SignalListener>, std::string>;

	TextureCache(alm::gfx::DataUploader* dataUploader, rhi::Device* device);
	~TextureCache();

	std::shared_ptr<LoadedTexture> Get(const std::string& id);

	LoadResult Load(const std::string& path, Flags flags);
	LoadResult Load(const alm::WeakBlob& blob, Flags flags = Flags::None, bool isDDS = false, const std::string& id = MakeUniqueStringId());

	void Update();

private:

	LoadResult LoadInternal(const alm::WeakBlob& blob, Flags flags, bool isDDS, const std::string& id);

	std::shared_ptr<LoadedTexture> CreateHandle();

	void RemoveStaleTextures();

private:

	struct InFlightData
	{
		std::string id;
		std::shared_ptr<LoadedTexture> texture;
		alm::SignalListener event;
	};

	std::unordered_map<std::string, std::weak_ptr<LoadedTexture>> m_Textures;
	std::vector<InFlightData> m_InFlightTextures;
	std::vector<std::string> m_StaleTextures;

	std::mutex m_MapMutex;
	std::mutex m_InFlightMapMutex;
	std::mutex m_StaleMapMutex;

	rhi::Device* m_Device;
	DataUploader* m_DataUploader;
};

} // namespace st::gfx

ENUM_CLASS_FLAG_OPERATORS(alm::gfx::TextureCache::Flags)