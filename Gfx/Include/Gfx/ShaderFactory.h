#pragma once

#include <unordered_map>
#include "Core/Blob.h"
#include "RHI/Shader.h"

namespace st::rhi
{
	class Device;
}

namespace st::gfx
{

class ShaderFactory
{
public:

	ShaderFactory(st::rhi::Device* device);

	st::rhi::ShaderHandle LoadShader(const char* filename, rhi::ShaderType shaderType);

private:

	std::unordered_map<std::string, st::Blob> m_BytecodeCache;
	rhi::Device* m_Device;
};

}