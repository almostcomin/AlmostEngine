#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <mutex>
#include <nvrhi/nvrhi.h>
#include "Core/Blob.h"
#include "Core/Util.h"

namespace st::gfx
{
	class DataUploader;
	class TextureHandle;
}

namespace st::gfx
{

class TextureCache
{
public:

	using LoadResult = std::expected<std::pair<std::shared_ptr<TextureHandle>, nvrhi::EventQueryHandle>, std::string>;

	TextureCache(nvrhi::DeviceHandle device, st::gfx::DataUploader* dataUploader);

	std::shared_ptr<TextureHandle> Get(const std::string& id);

	LoadResult Load(const std::string& path, bool forceSRGB);
	LoadResult Load(const st::WeakBlob& blob, const std::string& id = MakeUniqueStringId(), bool isDDS = false, bool forceSRGB = false);

	void Update();

private:

	std::expected<std::pair<std::shared_ptr<TextureHandle>, nvrhi::EventQueryHandle>, std::string>
	LoadInternal(const st::WeakBlob& blob, const std::string& key_path, bool isDDS);

	std::shared_ptr<TextureHandle> CreateHandle();

private:

	struct InFlightData
	{
		std::string id;
		std::shared_ptr<TextureHandle> texture;
		nvrhi::EventQueryHandle event;
	};

	std::unordered_map<std::string, std::weak_ptr<TextureHandle>> m_Textures;
	std::vector<InFlightData> m_InFlightTextures;
	std::vector<std::string> m_StaleTextures;

	std::mutex m_MapMutex;
	std::mutex m_InFlightMapMutex;
	std::mutex m_StaleMapMutex;

	nvrhi::DeviceHandle m_Device;
	DataUploader* m_DataUploader;
};

} // namespace st::gfx