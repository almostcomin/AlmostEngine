#pragma once

#include <unordered_map>
#include "Core/Blob.h"
#include "RenderAPI/Shader.h"

namespace st::rapi
{
	class Device;
}

namespace st::gfx
{

class ShaderFactory
{
public:

	ShaderFactory(st::rapi::Device* device);

	st::rapi::ShaderHandle CreateShader(const char* filename, st::rapi::ShaderType shaderType);

private:

	std::unordered_map<std::string, st::Blob> m_BytecodeCache;
	st::rapi::Device* m_Device;
};

}