#pragma once

#include <expected>
#include <string>
#include "Core/Memory.h"

namespace alm::gfx
{
	class SceneGraph;
	class DeviceManager;
};

namespace alm::gfx
{

std::expected<alm::unique<SceneGraph>, std::string>
ImportGlTF(const char* path, alm::gfx::DeviceManager* device);

}