#pragma once

#include "Core/Blob.h"
#include "RHI/Shader.h"
#include "RHI/ShaderModel.h"

namespace alm::rhi::ShaderCompiler
{

struct CompilationResult
{
	alm::Blob CompiledBlob;
	std::vector<std::string> Dependencies;
};

CompilationResult Compile(const std::string& shaderName, ShaderType shaderType, const alm::WeakBlob& srcData,
	const std::wstring& includeFolder, const std::string& entryPoint, bool debugMode, ShaderModel model = ShaderModel::SM_6_6);

std::string GetToolchainString(alm::rhi::ShaderType shaderType, alm::rhi::ShaderModel model, const std::string& entryPoint, bool debugMode);

} // namespace alm::rhi::ShaderCompiler