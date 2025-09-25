#pragma once

#include <expected>
#include <string>
#include "Core/Memory.h"

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

std::expected<st::unique<SceneGraph>, std::string>
ImportGlTF(const char* path, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device);

}