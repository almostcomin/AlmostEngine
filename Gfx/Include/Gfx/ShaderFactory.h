#pragma once

#include <unordered_map>
#include "Core/Blob.h"
#include "RHI/Shader.h"

namespace alm::rhi
{
	class Device;
}

namespace alm::gfx
{

class ShaderFactory
{
public:

	ShaderFactory(alm::rhi::Device* device);

	alm::rhi::ShaderOwner LoadShader(const std::string& name, rhi::ShaderType shaderType);

private:

	std::unordered_map<std::string, alm::Blob> m_BytecodeCache;
	rhi::Device* m_Device;
};

}