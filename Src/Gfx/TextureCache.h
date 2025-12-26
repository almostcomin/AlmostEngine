#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <mutex>
#include "Core/Blob.h"
#include "Core/Common.h"
#include "Core/Signal.h"

namespace st::rhi
{
	class Device;
}

namespace st::gfx
{
	class DataUploader;
	class LoadedTexture;
}

namespace st::gfx
{

class TextureCache
{
public:

	using LoadResult = std::expected<std::pair<std::shared_ptr<LoadedTexture>, st::SignalListener>, std::string>;

	TextureCache(rhi::Device* device, st::gfx::DataUploader* dataUploader);

	std::shared_ptr<LoadedTexture> Get(const std::string& id);

	LoadResult Load(const std::string& path, bool forceSRGB = false);
	LoadResult Load(const st::WeakBlob& blob, const std::string& id = MakeUniqueStringId(), bool isDDS = false, bool forceSRGB = false);

	void Update();

private:

	LoadResult LoadInternal(const st::WeakBlob& blob, const std::string& id, bool isDDS, bool forceSRGB);

	std::shared_ptr<LoadedTexture> CreateHandle();

private:

	struct InFlightData
	{
		std::string id;
		std::shared_ptr<LoadedTexture> texture;
		st::SignalListener event;
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