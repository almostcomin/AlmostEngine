#pragma once

#include "RenderAPI/Shader.h"
#include "RenderAPI/PipelineState.h"

namespace st::rapi
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

	CommonResources(ShaderFactory* shaderFactory, rapi::Device* device);
	~CommonResources();

	rapi::GraphicsPipelineStateHandle CreateBlitPSO(const rapi::FramebufferInfo& fbInfo);

private:

	ShaderFactory* m_ShaderFactory;
	rapi::Device* m_Device;

	rapi::ShaderHandle m_BlitVS;
	rapi::ShaderHandle m_BlitPS;
	rapi::GraphicsPipelineStateDesc m_BlitPSODesc;

};

} // namespace st::gfx