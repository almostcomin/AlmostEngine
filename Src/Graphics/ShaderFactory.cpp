#include "Graphics/ShaderFactory.h"
#include "Core/Log.h"
#include <fstream>

st::gfx::ShaderFactory::ShaderFactory(nvrhi::DeviceHandle device) : m_Device(device)
{
}


nvrhi::ShaderHandle st::gfx::ShaderFactory::CreateShader(const char* filename, nvrhi::ShaderType shaderType)
{
	std::span<uint8_t> bytecode;

	auto it = m_BytecodeCache.find(filename);
	if (it == m_BytecodeCache.end())
	{
		std::ifstream file(filename, std::ios::binary);
		if (!file.is_open())
		{
			st::log::Warning("ShaderManager::CreateShader: file {} not found.", filename);
			return nullptr;
		}
		file.seekg(0, std::ios::end);
		uint64_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> data(size);
		file.read((char*)data.data(), size);
		
		auto result = m_BytecodeCache.insert({ filename, std::move(data) });
		bytecode = result.first->second;
	}
	else
	{
		bytecode = it->second;
	}

	nvrhi::ShaderDesc shaderDesc{
		.shaderType = shaderType,
		.debugName = filename,
		.entryName = "main" 
	};

	return m_Device->createShader(shaderDesc, bytecode.data(), bytecode.size());
}