#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RenderAPI/Device.h"

st::gfx::ShaderFactory::ShaderFactory(st::rapi::Device* device) : m_Device(device)
{
}

st::rapi::ShaderHandle st::gfx::ShaderFactory::CreateShader(const char* filename, st::rapi::ShaderType shaderType)
{
	st::WeakBlob bytecode;

	auto it = m_BytecodeCache.find(filename);
	if (it == m_BytecodeCache.end())
	{
		st::fs::File file{ filename };
		if(!file.IsOpen())
		{
			LOG_ERROR("ShaderManager::CreateShader: file {} not found.", filename);
			return nullptr;
		}
		auto readResult = file.Read();
		assert(readResult);		

		auto result = m_BytecodeCache.insert({ filename, std::move(*readResult) });
		bytecode = st::WeakBlob{ result.first->second };
	}
	else
	{
		bytecode = st::WeakBlob{ it->second };
	}

	rapi::ShaderDesc shaderDesc{
		.Type = shaderType,
		.DebugName = filename,
		.EntryPoint = "main"
	};

	return m_Device->CreateShader(shaderDesc, bytecode);
}