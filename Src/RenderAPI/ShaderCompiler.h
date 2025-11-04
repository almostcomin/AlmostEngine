#pragma once

#include "Core/Blob.h"
#include "RenderAPI/Shader.h"

namespace st::rapi::ShaderCompiler
{

st::Blob Compile(ShaderType shaderType, const std::string& path, const std::string& entryPoint, bool debugMode, st::Blob* opt_rootSignature);

} // namespace st::rapi