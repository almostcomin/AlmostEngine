#pragma once

#include "Core/Blob.h"
#include "RHI/Shader.h"

namespace alm::rhi::ShaderCompiler
{

alm::Blob Compile(const std::string& shaderName, ShaderType shaderType, const alm::WeakBlob& srcData, const std::string& includeFolder,
	const std::string& entryPoint, bool debugMode);

} // namespace st::rhi