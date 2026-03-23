#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RHI/Device.h"
#include "RHI/ShaderCompiler.h"

alm::gfx::ShaderFactory::ShaderFactory(alm::rhi::Device* device) : m_Device(device)
{
}

alm::rhi::ShaderOwner alm::gfx::ShaderFactory::LoadShader(const std::string& name, alm::rhi::ShaderType shaderType)
{
	alm::WeakBlob cachedBytecode;

	auto it = m_BytecodeCache.find(name);
	if (it == m_BytecodeCache.end())
	{
		std::filesystem::path srcPath = SHADERS_SRC_FOLDER;
		srcPath /= (name + ".hlsl");
		std::filesystem::path binPath = std::filesystem::current_path() / "Shaders" / (name + ".bin");

		const bool srcExists = std::filesystem::exists(srcPath);
		const bool binExists = std::filesystem::exists(binPath);
		bool compileShader = false;

		if (!srcExists)
		{
			LOG_WARNING("Source shader '{}' not found", srcPath.string());
		}

		if (!binExists)
		{
			compileShader = true;
		}
		else if (srcExists)
		{
			auto sourceTime = std::filesystem::last_write_time(srcPath);
			auto binTime = std::filesystem::last_write_time(binPath);
			
			// Actually we should check also all the chain of include files to check if any of them has changed...
			// for the moment, recompile always
			compileShader = true;//compileShader = sourceTime > binTime;
		}

		alm::Blob byteCode;
		// Load bin if compilation is not needed
		if (!compileShader)
		{
			alm::fs::File file{ binPath.string() };
			if (file.IsOpen())
			{
				auto readResult = file.Read();
				assert(readResult);
				byteCode = std::move(*readResult);
			}
		}
		else
		{
			fs::File srcFile{ srcPath.string() };
			if (!srcFile.IsOpen())
			{
				LOG_ERROR("Shader source file '{}' not found", srcPath.string());
			}
			else
			{
				auto readResult = srcFile.Read();
				assert(readResult);
				srcFile.Close();

				LOG_INFO("Compiling shader '{}'...", srcPath.string());
				auto startTime = std::chrono::steady_clock::now();
				byteCode = alm::rhi::ShaderCompiler::Compile(srcPath.filename().string(), shaderType, alm::WeakBlob{ *readResult }, SHADERS_SRC_FOLDER, "main", true);
				auto elapsed = std::chrono::steady_clock::now() - startTime;

				if (byteCode)
				{
					LOG_INFO("Shader '{}' compiled OK in {} ms", srcPath.string(), std::chrono::duration<float>(elapsed) * 1000.f);

					// Save bin
					fs::File binFile{ binPath.string(), fs::OpenMode::Write };
					assert(binFile.IsOpen());
					auto writeResult = binFile.Write(byteCode.data(), byteCode.size());
					if (!writeResult)
					{
						LOG_ERROR("Faileds writing shader file '{}'", binPath.string());
					}
				}
				else
				{
					LOG_ERROR("Failed shader compilation '{}'", srcPath.string());
				}
			}
		}

		if (byteCode)
		{
			auto result = m_BytecodeCache.insert({ name, std::move(byteCode) });
			cachedBytecode = alm::WeakBlob{ result.first->second };
		}
	}
	else // Bytecode already cached
	{
		cachedBytecode = alm::WeakBlob{ it->second };
	}

	if (cachedBytecode)
	{
		rhi::ShaderDesc shaderDesc{
			.Type = shaderType,
			.EntryPoint = "main"
		};

		return m_Device->CreateShader(shaderDesc, cachedBytecode, name);
	}
	else
	{
		LOG_ERROR("Failed to load shader '{}'", name);
		return {};
	}
}