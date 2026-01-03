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

	rhi::GraphicsPipelineStateOwner CreateBlitPSO(const rhi::FramebufferInfo& fbInfo);

	rhi::ShaderHandle GetFullscreenTriangleVS() const;

private:

	ShaderFactory* m_ShaderFactory;
	rhi::Device* m_Device;

	rhi::ShaderOwner m_BlitVS;
	rhi::ShaderOwner m_BlitPS;
	rhi::GraphicsPipelineStateDesc m_BlitPSODesc;

	rhi::ShaderOwner m_FullscreenTriangleVS;
};

} // namespace st::gfx