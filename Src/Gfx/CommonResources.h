#pragma once

#include "RHI/Shader.h"
#include "RHI/PipelineState.h"

namespace st::rhi
{
	class Device;
	struct FramebufferInfo;
};

namespace st::gfx
{
	class ShaderFactory;
};

namespace st::gfx
{

class CommonResources
{
public:

	CommonResources(ShaderFactory* shaderFactory, rhi::Device* device);
	~CommonResources();

	rhi::GraphicsPipelineStateHandle CreateBlitPSO(const rhi::FramebufferInfo& fbInfo);

private:

	ShaderFactory* m_ShaderFactory;
	rhi::Device* m_Device;

	rhi::ShaderHandle m_BlitVS;
	rhi::ShaderHandle m_BlitPS;
	rhi::GraphicsPipelineStateDesc m_BlitPSODesc;

};

} // namespace st::gfx