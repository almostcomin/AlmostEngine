#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <expected>
#include <nvrhi/nvrhi.h>

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
		State state = State::Unloaded;
	};

public:

	TextureCache(nvrhi::DeviceHandle device);

	std::expected<std::shared_ptr<Handle>, std::string> Load(const std::string& path);
//	std::expected<std::shared_ptr<Handle>, std::string> LoadAsync(const std::filesystem::path& path);

private:

	std::unordered_map<std::string, std::weak_ptr<Handle>> m_Textures;
	
	nvrhi::DeviceHandle m_Device;
};

} // namespace st::gfx