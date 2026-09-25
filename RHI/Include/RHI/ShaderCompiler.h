#pragma once

#include "Core/Blob.h"
#include "RHI/Shader.h"
#include "RHI/ShaderModel.h"

namespace alm::rhi::ShaderCompiler
{

alm::Blob Compile(const std::string& shaderName, ShaderType shaderType, const alm::WeakBlob& srcData, const std::string& includeFolder,
	const std::string& entryPoint, bool debugMode, ShaderModel model = ShaderModel::SM_6_6);

} // namespace alm::rhi::ShaderCompiler