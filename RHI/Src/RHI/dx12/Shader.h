#pragma once

#include "RHI/Shader.h"

namespace alm::rhi::dx12
{

class Shader : public IShader
{
public:

	Shader(const ShaderDesc& desc, const WeakBlob& bytecode, Device* device, const std::string& debugName) :
		IShader{ device, debugName }, m_Desc { desc }, m_Bytecode{ bytecode }
	{}

	const ShaderDesc& GetDesc() const override { return m_Desc; }
	const WeakBlob& GetBytecode() const override { return m_Bytecode; }

	void Release(Device* device) override {}

private:

	ShaderDesc m_Desc;
	WeakBlob m_Bytecode;
};

} // namespace st::rhi::dx12