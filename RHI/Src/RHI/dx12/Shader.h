#pragma once

#include "RHI/Shader.h"

namespace st::rhi::dx12
{

class Shader : public IShader
{
public:

	Shader(const ShaderDesc& desc, const WeakBlob& bytecode) : m_Desc{ desc }, m_Bytecode{ bytecode }
	{}

	const ShaderDesc& GetDesc() const override { return m_Desc; }
	const WeakBlob& GetBytecode() const override { return m_Bytecode; }

	const std::string& GetDebugName() override { return m_Desc.DebugName; }

	void Release(Device* device) override {}

private:

	ShaderDesc m_Desc;
	WeakBlob m_Bytecode;
};

} // namespace st::rhi::dx12