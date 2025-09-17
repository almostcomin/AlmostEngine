#pragma once

#include <expected>
#include <memory>
#include <string>

namespace st::gfx
{
	class SceneGraph;
	class DataUploader;
};

namespace nvrhi
{
	class IDevice;
}

namespace st::gfx
{

std::expected<std::unique_ptr<SceneGraph>, std::string> 
ImportGlTF(const char* path, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device);

}