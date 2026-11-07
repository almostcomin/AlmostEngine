#include "Gfx/GfxPCH.h"
#include "Gfx/ShaderFactory.h"
#include "Core/Log.h"
#include "Core/File.h"
#include "RHI/Device.h"
#include "RHI/ShaderCompiler.h"

static bool IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static std::string_view TrimToken(std::string_view token)
{
	while (!token.empty() && IsSpace(token.front()))
		token.remove_prefix(1);
	while (!token.empty() && IsSpace(token.back()))
		token.remove_suffix(1);
	return token;
}

static std::string_view OccurrenceLine(std::string_view src, size_t occurrence)
{
	size_t lineStart = src.rfind('\n', occurrence);
	lineStart = (lineStart == std::string_view::npos) ? 0 : lineStart + 1;
	size_t lineEnd = src.find('\n', occurrence);
	lineEnd = (lineEnd == std::string_view::npos) ? src.size() : lineEnd;
	if (lineStart >= lineEnd)
		return {};
	std::string_view line{ &src[lineStart], lineEnd - lineStart };
	if (!line.empty() && line.back() == '\r')
		line.remove_suffix(1);
	return line;
}

// Canonical define spelling: a naked define implicitly carries value 1, so both forms are
// equivalent and must share compile arguments, cache keys and .bin/.deps file names
static std::string CanonizeDefine(const std::string& define)
{
	std::string canon{ define };
	if (canon.find('=') == std::string::npos)
		canon += "=1";
	return canon;
}

// Shader model requirements declared by the shader with ALM_REQUIRE_SM(SM[; GATE[, GATE]...]).
// Gates match active variants by define name (e.g. "GPU_CULL" matches "GPU_CULL=1" or naked
// "GPU_CULL", but not "GPU_CULL=0"); all gates must match to apply, no gates always applies.
// The highest applicable model wins; fallback is returned when no requirement applies.
static alm::rhi::ShaderModel ParseRequiredModel(alm::WeakBlob src, const std::string& shaderName, const std::vector<std::string>& variants,
	alm::rhi::ShaderModel fallback)
{
	const std::string_view src_view{ reinterpret_cast<const char*>(src.data()), src.size() };
	constexpr std::string_view prefix = "ALM_REQUIRE_SM(";
	alm::rhi::ShaderModel result = fallback;

	auto rank = [](alm::rhi::ShaderModel model)
	{
		switch (model)
		{
		case alm::rhi::ShaderModel::SM_6_8: return 2;
		case alm::rhi::ShaderModel::SM_6_6: return 1;
		default: return 0;
		}
	};

	// Define name (the part before '=')
	auto defineName = [](std::string_view define)
	{
		const size_t eq = define.find('=');
		return (eq != std::string_view::npos) ? define.substr(0, eq) : define;
	};

	// Gates only match active variants: value != 0 (a naked define counts as 1).
	// Disabled injected defines (e.g. NAME=0) must not trigger gates.
	auto isVariantActive = [](std::string_view define)
	{
		const size_t eq = define.find('=');
		if (eq == std::string_view::npos)
			return true;
		const std::string_view value = define.substr(eq + 1);
		return value.find_first_not_of(" \t\r\n0") != std::string_view::npos;
	};

	// Gate tokens match by define name (the part before '=')
	// e.g. gate "GPU_CULL" matches variant "GPU_CULL=1" or naked "GPU_CULL", but not "GPU_CULL=0"
	auto variantMatches = [&variants, &defineName, &isVariantActive](std::string_view req)
	{
		req = defineName(req);
		for (const std::string& variant : variants)
		{
			if (defineName(variant) == req && isVariantActive(variant))
				return true;
		}
		return false;
	};

	size_t pos = src_view.find(prefix);
	while (pos != std::string::npos)
	{
		const size_t occurrencePos = pos;
		size_t cursor = pos + prefix.size();
		pos = src_view.find(prefix, pos + prefix.size()); // Next occurrence, if any

		alm::rhi::ShaderModel candidate{};
		if (src_view.compare(cursor, 3, "6.6") == 0)
		{
			candidate = alm::rhi::ShaderModel::SM_6_6;
			cursor += 3;
		}
		else if (src_view.compare(cursor, 3, "6.8") == 0)
		{
			candidate = alm::rhi::ShaderModel::SM_6_8;
			cursor += 3;
		}
		else
		{
			// Malformed or not supported SM
			LOG_WARNING("'{}': Ignoring 'ALM_REQUIRE_SM': Malformed or invalid shader model. Line: '{}'",
				shaderName, OccurrenceLine(src_view, occurrencePos));
			continue;
		}

		// Gates are optional, separated by ';' or ','. All of them must match to apply.
		// A requirement without gates always applies.
		const size_t close = src_view.find(')', cursor);
		const size_t end = (close != std::string_view::npos) ? close : src_view.size();

		bool applicable = true;
		while (cursor < end)
		{
			while (cursor < end && IsSpace(src_view[cursor]))
				++cursor;
			if (cursor >= end)
				break;

			const size_t delim = src_view.find_first_of(";,)", cursor);
			if (delim == std::string_view::npos || delim > end)
			{
				LOG_WARNING("'{}': Ignoring 'ALM_REQUIRE_SM({})': Malformed gate. Line: '{}'",
					shaderName, GetShaderModelString(candidate), OccurrenceLine(src_view, occurrencePos));
				applicable = false;
				break;
			}

			std::string_view token = TrimToken({ &src_view[cursor], delim - cursor });

			if (!token.empty() && !variantMatches(token))
				applicable = false;

			cursor = delim + 1;
		}

		if (applicable && rank(candidate) > rank(result))
			result = candidate;
	}
	return result;
}

// Defines declared by the shader with ALM_REQUIRE_VARIANT(NAME[, NAME=value]...)
// Tokens are returned as declared ("NAME" or "NAME=value"); a naked NAME is injected as NAME=0 when
// missing from the caller's variant list (see LoadShader).
static std::pair<std::vector<std::string>, bool> ParseRequiredVariants(alm::WeakBlob src, const std::string& shaderName)
{
	std::vector<std::string> declared;
	bool ok = true;

	const std::string_view src_view{ reinterpret_cast<const char*>(src.data()), src.size() };
	constexpr std::string_view prefix = "ALM_REQUIRE_VARIANT(";

	auto defineName = [](std::string_view define)
	{
		const size_t eq = define.find('=');
		return (eq != std::string_view::npos) ? define.substr(0, eq) : define;
	};

	auto findDeclared = [&declared, &defineName](std::string_view name)
	{
		return std::ranges::find_if(declared, [name, &defineName](const std::string& d)
		{
			return defineName(d) == name;
		});
	};

	size_t pos = src_view.find(prefix);
	while (pos != std::string::npos)
	{
		const size_t occurrencePos = pos;
		size_t cursor = pos + prefix.size();
		pos = src_view.find(prefix, pos + prefix.size()); // Next occurrence, if any

		const size_t close = src_view.find(')', cursor);
		const size_t end = (close != std::string_view::npos) ? close : src_view.size();

		while (cursor < end)
		{
			while (cursor < end && IsSpace(src_view[cursor]))
				++cursor;
			if (cursor >= end)
				break;

			const size_t delim = src_view.find_first_of(";,)", cursor);
			if (delim == std::string_view::npos || delim > end)
			{
				LOG_WARNING("'{}': Ignoring 'ALM_REQUIRE_VARIANT': Malformed variant list. Line: '{}'",
					shaderName, OccurrenceLine(src_view, occurrencePos));
				break;
			}

			std::string_view token = TrimToken({ &src_view[cursor], delim - cursor });
			cursor = delim + 1;
			if (token.empty())
				continue;

			const std::string define{ token };
			auto it = findDeclared(token);
			if (it != declared.end())
			{
				if (*it != define)
				{
					LOG_ERROR("'{}': Conflicting values in 'ALM_REQUIRE_VARIANT' for '{}'. Line: '{}'",
						shaderName, defineName(token), OccurrenceLine(src_view, occurrencePos));
					ok = false;
				}
			}
			else
			{
				declared.push_back(std::move(define));
			}
		}
	}

	return { std::move(declared), ok };
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

alm::rhi::ShaderOwner alm::gfx::ShaderFactory::LoadShader(const std::string& name, alm::rhi::ShaderType shaderType,
	const std::vector<std::string>& variant)
{
	std::vector<std::string> sortedVar = variant;
	// Canonical spelling first, so "NAME" and "NAME=1" dedup into one define
	for (std::string& v : sortedVar)
		v = CanonizeDefine(v);
	std::ranges::sort(sortedVar);
	{
		const auto [first, last] = std::ranges::unique(sortedVar);
		if (first != last)
		{
			LOG_WARNING("'{}': Duplicated shader defines removed from variant", name);
		}
		sortedVar.erase(first, last);
	}

	// Define name (the part before '=')
	const auto defineName = [](std::string_view v)
	{
		const size_t eq = v.find('=');
		return (eq != std::string_view::npos) ? v.substr(0, eq) : v;
	};

	// Same define name with different values is a caller error
	for (size_t i = 1; i < sortedVar.size(); ++i)
	{
		const std::string_view prev{ sortedVar[i - 1] };
		const std::string_view curr{ sortedVar[i] };
		if (defineName(prev) == defineName(curr) && prev != curr)
		{
			LOG_ERROR("'{}': Conflicting define values in variant: '{}' vs '{}'", name, prev, curr);
			return {};
		}
	}

	std::string nameWithVar = name;
	for (const std::string& v : sortedVar)
	{
		nameWithVar += "_" + v;
	}

	alm::WeakBlob cachedBytecode;

	auto it = m_BytecodeCache.find(nameWithVar);
	if (it == m_BytecodeCache.end())
	{
		std::filesystem::path srcPath = SHADERS_SRC_FOLDER;
		srcPath /= (name + ".hlsl");

		std::filesystem::path binPath = std::filesystem::current_path() / 
			"_shaders" / 
			(m_ShadersDebug ? "_debug" : "_release") /
			(nameWithVar + ".bin");

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

		rhi::ShaderModel shaderModel = ParseRequiredModel(
			alm::WeakBlob{ *readResult }, srcPath.filename().string(), sortedVar, rhi::ShaderModel::SM_6_6);
		if (!m_Device->IsShaderModelSupported(shaderModel))
		{
			LOG_ERROR("Error on shader '{}': Required SM {} not supported", srcPath.string(), GetShaderModelString(shaderModel));
			return {};
		}

		const auto [declaredVars, declaredOk] = ParseRequiredVariants(alm::WeakBlob{ *readResult }, srcPath.filename().string());
		if (!declaredOk)
			return {};

		// Expanded variant list: caller's defines plus declared defines missing from it.
		// Used for compiling arguments and toolchain string (cache invalidation),
		// while the caller's owns the suffix, cache key and SM gates.
		std::vector<std::string> expandedVar = sortedVar;
		if (!declaredVars.empty())
		{
			// A caller define not declared by the shader is a caller error (typo?)
			for (const std::string& v : sortedVar)
			{
				const bool declared = std::ranges::any_of(declaredVars, [&v, &defineName](const std::string& d)
				{
					return defineName(d) == defineName(v);
				});
				if (!declared)
				{
					LOG_ERROR("'{}': Define '{}' not declared by the shader with 'ALM_REQUIRE_VARIANT'", name, v);
					return {};
				}
			}

			// Inject declared defines missing from the caller's list
			for (const std::string& declared : declaredVars)
			{
				const bool present = std::ranges::any_of(sortedVar, [&declared, &defineName](const std::string& v)
				{
					return defineName(v) == defineName(declared);
				});
				if (!present)
				{
					std::string injected = declared;
					if (injected.find('=') == std::string::npos)
						injected += "=0";
					expandedVar.push_back(std::move(injected));
				}
			}
			std::ranges::sort(expandedVar);
		}

		std::string toolchainString = alm::rhi::ShaderCompiler::GetToolchainString(
			shaderType, shaderModel, "main", m_ShadersDebug, expandedVar);

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
				"main", m_ShadersDebug, expandedVar, shaderModel);
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
				SaveDepsFile(depsPath, toolchainString, compilationResult.Dependencies);
			}
			else
			{
				LOG_ERROR("Failed shader compilation '{}'", srcPath.string());
			}

		}

		if (byteCode)
		{
			auto result = m_BytecodeCache.insert({ nameWithVar, std::move(byteCode) });
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

		return m_Device->CreateShader(shaderDesc, cachedBytecode, nameWithVar);
	}
	else
	{
		LOG_ERROR("Failed to load shader '{}'", nameWithVar);
		return {};
	}
}
