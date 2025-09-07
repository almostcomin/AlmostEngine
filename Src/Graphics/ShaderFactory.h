#pragma once

#include "Core/Core.h"
#include <nvrhi/nvrhi.h>

namespace st::gfx
{

class ShaderFactory
{
public:

	ShaderFactory(nvrhi::DeviceHandle device);

	nvrhi::ShaderHandle CreateShader(const char* filename, nvrhi::ShaderType shaderType);

private:

	std::unordered_map<std::string, std::vector<uint8_t>> m_BytecodeCache;
	nvrhi::DeviceHandle m_Device;
};

}