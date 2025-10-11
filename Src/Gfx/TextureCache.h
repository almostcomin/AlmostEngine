#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <mutex>
#include <nvrhi/nvrhi.h>


namespace st::gfx
{
	class DataUploader;
}

namespace st::gfx
{

class TextureCache
{
public:

	struct Handle
	{
		enum State
		{
			Unloaded,
			Loading,
			Loaded
		};

		nvrhi::TextureHandle texture;
		std::string path;
		State state = State::Unloaded;
	};

public:

	TextureCache(nvrhi::DeviceHandle device, st::gfx::DataUploader* dataUploader);

	std::expected<std::shared_ptr<Handle>, std::string> Load(const std::string& path);

	void Update();

private:

	std::shared_ptr<Handle> CreateHandle();

private:

	std::unordered_map<std::string, std::weak_ptr<Handle>> m_Textures;
	std::unordered_map<std::string, std::pair<std::shared_ptr<Handle>, nvrhi::EventQueryHandle>> m_InFlightTextures;
	std::vector<std::string> m_StaleTextures;

	std::mutex m_MapMutex;
	std::mutex m_InFlightMapMutex;
	std::mutex m_StaleMapMutex;

	nvrhi::DeviceHandle m_Device;
	DataUploader* m_DataUploader;
};

} // namespace st::gfx