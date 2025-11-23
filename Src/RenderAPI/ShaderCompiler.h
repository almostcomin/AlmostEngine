#pragma once

#include "Core/Blob.h"
#include "RenderAPI/Shader.h"

namespace st::rapi::ShaderCompiler
{

st::Blob Compile(ShaderType shaderType, const st::WeakBlob& srcData, const std::string& includeFolder, const std::string& entryPoint, 
	bool debugMode);

} // namespace st::rapi