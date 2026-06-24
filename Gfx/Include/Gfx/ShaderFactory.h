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

	ShaderFactory(bool shadersDebug, alm::rhi::Device* device);

	alm::rhi::ShaderOwner LoadShader(const std::string& name, rhi::ShaderType shaderType);

private:

	std::unordered_map<std::string, alm::Blob> m_BytecodeCache;

	bool m_ShadersDebug;
	rhi::Device* m_Device;
};

}