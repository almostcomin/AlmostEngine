#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <mutex>
#include "Core/Blob.h"
#include "Core/Common.h"
#include "Core/Signal.h"
#include "Gfx/TextureLoadFlags.h"

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

	enum class FileFormat
	{
		DDS,
		KTX,
		Other
	};

	using LoadResult = std::expected<std::pair<std::shared_ptr<LoadedTexture>, alm::SignalListener>, std::string>;

	TextureCache(alm::gfx::DataUploader* dataUploader, rhi::Device* device);
	~TextureCache();

	std::shared_ptr<LoadedTexture> Get(const std::string& id);

	LoadResult Load(const std::string& path, TextureLoadFlags flags);
	LoadResult Load(const alm::WeakBlob& blob, FileFormat format, TextureLoadFlags flags = TextureLoadFlags::None,
		const std::string& id = MakeUniqueStringId());

	void Update();

	static FileFormat GetFileFormatFromExtension(std::string_view ext);

private:

	LoadResult LoadInternal(const alm::WeakBlob& blob, alm::gfx::TextureLoadFlags flags, FileFormat format, const std::string& id);

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