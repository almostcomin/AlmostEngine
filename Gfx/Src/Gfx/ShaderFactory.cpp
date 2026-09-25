#include "Gfx/GfxPCH.h"
#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RHI/Device.h"
#include "RHI/ShaderCompiler.h"

static alm::rhi::ShaderModel ParseRequiredModel(alm::WeakBlob src, alm::rhi::ShaderModel fallback)
{
	const std::string_view src_view{ reinterpret_cast<const char*>(src.data()), src.size() };
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

static void SaveDepsFile(const std::filesystem::path& depsPath, const std::string& toolchainString, const std::vector<std::string>& deps)
{
	// Save deps
	alm::fs::File depsFile{ depsPath.string(), alm::fs::OpenMode::Write };
	assert(depsFile.IsOpen());

	auto writeResult = depsFile.WriteLine(toolchainString);
	assert(writeResult);

	for (const auto& depString : deps)
	{
		writeResult = depsFile.WriteLine(depString);
		assert(writeResult);
	}
	depsFile.Close();
}

static std::pair<std::string, std::vector<std::string>> LoadDepsFile(const std::filesystem::path& depsPath)
{
	alm::fs::File depsFile{ depsPath.string(), alm::fs::OpenMode::Read };
	if (!depsFile.IsOpen())
		return {};

	auto readResult = depsFile.ReadLine();
	if (!readResult)
		return {};
	std::string toolchain = std::move(*readResult);

	std::vector<std::string> deps;
	while (true)
	{
		readResult = depsFile.ReadLine();
		if (!readResult)
			break;
		deps.emplace_back(std::move(*readResult));
	}

	return { toolchain, deps };
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
		std::filesystem::path depsPath = binPath;
		depsPath.replace_extension(".deps");

		const bool srcExists = std::filesystem::exists(srcPath);
		const bool binExists = std::filesystem::exists(binPath);
		const bool depsExists = std::filesystem::exists(depsPath);
		bool compileShader = false;

		if (!srcExists)
		{
			LOG_ERROR("Source shader '{}' not found", srcPath.string());
			return {};
		}

		fs::File srcFile{ srcPath.string() };
		auto readResult = srcFile.Read();
		if (!readResult)
		{
			LOG_ERROR("Failed loading shader file '{}', error: {}", srcPath.string(), readResult.error());
			return {};
		}
		srcFile.Close();

		rhi::ShaderModel shaderModel = ParseRequiredModel(alm::WeakBlob{ *readResult }, rhi::ShaderModel::SM_6_6);
		if (!m_Device->IsShaderModelSupported(shaderModel))
		{
			LOG_ERROR("Error on shader '{}': Required SM {} not supported", srcPath.string(), GetShaderModelString(shaderModel));
			return {};
		}

		std::string toolchainString = alm::rhi::ShaderCompiler::GetToolchainString(
			shaderType, shaderModel, "main", m_ShadersDebug);

		bool cacheValid = binExists && depsExists;
		if (!cacheValid)
		{
			compileShader = true;
		}
		else
		{
			auto binTime = std::filesystem::last_write_time(binPath);
			auto depsTime = std::filesystem::last_write_time(depsPath);
			auto sourceTime = std::filesystem::last_write_time(srcPath);

			if (sourceTime > binTime || sourceTime > depsTime)
			{
				compileShader = true;
			}
			else
			{
				auto deps = LoadDepsFile(depsPath);
				if (deps.first != toolchainString)
				{
					compileShader = true;
				}
				else
				{
					for (const auto& dep : deps.second)
					{
						std::error_code ec;
						auto depTime = std::filesystem::last_write_time(dep, ec);
						if (ec || depTime > binTime || depTime > depsTime)
						{
							compileShader = true;
							break;
						}
					}
				}
			}
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
				
				LOG_INFO("Shader '{}' loaded from cache", binPath.string());
			}
		}
		else
		{
			auto startTime = std::chrono::steady_clock::now();
			rhi::ShaderCompiler::CompilationResult compilationResult = alm::rhi::ShaderCompiler::Compile(
				srcPath.filename().string(), shaderType, alm::WeakBlob{ *readResult }, ToWide(SHADERS_SRC_FOLDER),
				"main", m_ShadersDebug, shaderModel);
			auto elapsed = std::chrono::steady_clock::now() - startTime;

			if (compilationResult.CompiledBlob)
			{
				LOG_INFO("Shader '{}' compiled OK in {} ms", srcPath.string(), std::chrono::duration<float>(elapsed) * 1000.f);
				byteCode = std::move(compilationResult.CompiledBlob);

				// Save bin
				fs::File binFile{ binPath.string(), fs::OpenMode::Write };
				assert(binFile.IsOpen());
				auto writeResult = binFile.Write(byteCode.data(), byteCode.size());
				if (!writeResult)
				{
					LOG_ERROR("Faileds writing shader file '{}'", binPath.string());
				}
				binFile.Close();

				// Save deps
				auto depsPath = binPath;
				depsPath.replace_extension(".deps");
				SaveDepsFile(depsPath, toolchainString, compilationResult.Dependencies);
			}
			else
			{
				LOG_ERROR("Failed shader compilation '{}'", srcPath.string());
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