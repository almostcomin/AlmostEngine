#pragma once

#include "Core/Blob.h"
#include "RHI/Shader.h"

namespace st::rhi::ShaderCompiler
{

st::Blob Compile(const std::string& shaderName, ShaderType shaderType, const st::WeakBlob& srcData, const std::string& includeFolder,
	const std::string& entryPoint, bool debugMode);

} // namespace st::rhi