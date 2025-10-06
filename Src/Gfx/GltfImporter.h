#pragma once

#include <expected>
#include <string>
#include "Core/Memory.h"

namespace st::gfx
{
	class SceneGraph;
	class DeviceManager;
};

namespace st::gfx
{

std::expected<st::unique<SceneGraph>, std::string>
ImportGlTF(const char* path, st::gfx::DeviceManager* device);

}