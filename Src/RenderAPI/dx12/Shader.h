#pragma once

#include "RenderAPI/Shader.h"

namespace st::rapi::dx12
{

class Shader : public IShader
{
public:

	Shader(const ShaderDesc& desc, const WeakBlob& bytecode) : m_Desc{ desc }, m_Bytecode{ bytecode }
	{}

	const ShaderDesc& GetDesc() const override { return m_Desc; }
	const WeakBlob& GetBytecode() const override { return m_Bytecode; }

private:

	ShaderDesc m_Desc;
	WeakBlob m_Bytecode;
};

} // namespace st::rapi::dx12