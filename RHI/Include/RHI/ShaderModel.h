#pragma once

#include "Core/Blob.h"
#include "RHI/Shader.h"

namespace alm::rhi
{
enum class ShaderModel
{
	SM_6_6,
	SM_6_8
};

inline const char* GetShaderModelString(ShaderModel sm)
{
	switch (sm)
	{
	case ShaderModel::SM_6_6: return "6.6";
	case ShaderModel::SM_6_8: return "6.8";
	default:
		assert(0);
		return "<unknown>";
	}
}

} // namespace alm::rhi
