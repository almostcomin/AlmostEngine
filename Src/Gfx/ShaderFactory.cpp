#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/ShaderCompiler.h"

st::gfx::ShaderFactory::ShaderFactory(st::rapi::Device* device) : m_Device(device)
{
}

st::rapi::ShaderHandle st::gfx::ShaderFactory::LoadShader(const char* filename, st::rapi::ShaderType shaderType)
{
	st::WeakBlob cachedBytecode;

	auto it = m_BytecodeCache.find(filename);
	if (it == m_BytecodeCache.end())
	{
		st::Blob byteCode;
/*
		st::fs::File file{ filename };
		if (file.IsOpen())
		{
			auto readResult = file.Read();
			assert(readResult);
			byteCode = std::move(*readResult);
		}
		else
*/
		st::fs::File file;
		{
			std::filesystem::path shaderSourcePath = SHADERS_SRC_FOLDER;
			shaderSourcePath /= filename;
			shaderSourcePath.replace_extension(".hlsl");
			fs::File srcFile{ shaderSourcePath.string() };
			if (!srcFile.IsOpen())
			{
				LOG_ERROR("Shader source file '{}' not found", shaderSourcePath.string());
			}
			else
			{
				auto readResult = srcFile.Read();
				assert(readResult);
				st::Blob pdbData;
				std::wstring pdbName;
				byteCode = st::rapi::ShaderCompiler::Compile(shaderType, st::WeakBlob{ *readResult }, SHADERS_SRC_FOLDER, "main", true);
				if (byteCode)
				{
					// Save compìled shader and pdb if present
					file.Open(filename, fs::OpenMode::Write);
					assert(file.IsOpen());
					file.Write(byteCode.data(), byteCode.size());
					file.Close();
				}
				else
				{
					LOG_ERROR("Failed shader compilation '{}'", shaderSourcePath.string());
				}
			}
		}

		if (byteCode)
		{
			auto result = m_BytecodeCache.insert({ filename, std::move(byteCode) });
			cachedBytecode = st::WeakBlob{ result.first->second };
		}
	}
	else
	{
		cachedBytecode = st::WeakBlob{ it->second };
	}

	if (cachedBytecode)
	{
		rapi::ShaderDesc shaderDesc{
			.Type = shaderType,
			.DebugName = filename,
			.EntryPoint = "main"
		};

		return m_Device->CreateShader(shaderDesc, cachedBytecode);
	}
	else
	{
		return {};
	}
}