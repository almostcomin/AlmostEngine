#include "Gfx/GfxPCH.h"
#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RHI/Device.h"
#include "RHI/ShaderCompiler.h"

static alm::rhi::ShaderModel ParseRequiredModel(alm::WeakBlob src, alm::rhi::ShaderModel fallback)
{
	const std::string_view src_view(reinterpret_cast<const char*>(src.data()), src.size());
	constexpr std::string_view prefix = "ALM_REQUIRE_SM(";

	size_t pos = src_view.find("ALM_REQUIRE_SM(");
	if (pos == std::string::npos)
		return fallback;

	size_t vpos = pos + prefix.size();
	if (vpos + 3 > src_view.size())
		return fallback;

	if(src_view.compare(vpos, 3, "6.6") == 0)
		return alm::rhi::ShaderModel::SM_6_6;
	if(src_view.compare(vpos, 3, "6.8") == 0)
		return alm::rhi::ShaderModel::SM_6_8;

	return fallback;
}

alm::gfx::ShaderFactory::ShaderFactory(bool shadersDebug, alm::rhi::Device* device) : 
	m_ShadersDebug{ shadersDebug }, m_Device(device)
{}

alm::rhi::ShaderOwner alm::gfx::ShaderFactory::LoadShader(const std::string& name, alm::rhi::ShaderType shaderType)
{
	alm::WeakBlob cachedBytecode;

	auto it = m_BytecodeCache.find(name);
	if (it == m_BytecodeCache.end())
	{
		std::filesystem::path srcPath = SHADERS_SRC_FOLDER;
		srcPath /= (name + ".hlsl");
		std::filesystem::path binPath = std::filesystem::current_path() / 
			"_shaders" / 
			(m_ShadersDebug ? "_debug" : "_release") /
			(name + ".bin");

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
			//auto sourceTime = std::filesystem::last_write_time(srcPath);
			//auto binTime = std::filesystem::last_write_time(binPath);
			
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

				rhi::ShaderModel sm = ParseRequiredModel(alm::WeakBlob{ *readResult }, rhi::ShaderModel::SM_6_6);
				if(!m_Device->IsShaderModelSupported(sm))
				{
					LOG_ERROR("Failed shader compilation '{}': Required SM {} not supported", srcPath.string(), GetShaderModelString(sm));
					return {};
				}

				LOG_INFO("Compiling shader '{}'...", srcPath.string());
				auto startTime = std::chrono::steady_clock::now();
				byteCode = alm::rhi::ShaderCompiler::Compile(srcPath.filename().string(), shaderType, alm::WeakBlob{ *readResult },
					SHADERS_SRC_FOLDER, "main", m_ShadersDebug, sm);
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