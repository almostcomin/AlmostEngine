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

	rhi::GraphicsPipelineStateOwner CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo);
	rhi::GraphicsPipelineStateOwner CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo, const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS,
		const std::string debugName);

	rhi::ComputePipelineStateHandle GetBlitComputePSO() { return m_BlitComputePSO.get_weak(); }

	rhi::ComputePipelineStateHandle GetClearBufferPSO() { return m_ClearBufferPSO.get_weak(); }

	rhi::ShaderHandle GetBlitVS() const { return m_BlitVS.get_weak(); }
	rhi::ShaderHandle GetBlitPS() const { return m_BlitPS.get_weak(); }
	rhi::ShaderHandle GetClearBufferCS() const { return m_ClearBufferCS.get_weak(); }

private:

	ShaderFactory* m_ShaderFactory;
	rhi::Device* m_Device;

	rhi::ShaderOwner m_BlitVS;
	rhi::ShaderOwner m_BlitPS;
	rhi::GraphicsPipelineStateDesc m_BlitGraphicsPSODesc;

	rhi::ShaderOwner m_BlitCS;
	rhi::ComputePipelineStateOwner m_BlitComputePSO;

	rhi::ShaderOwner m_ClearBufferCS;
	rhi::ComputePipelineStateOwner m_ClearBufferPSO;
};

} // namespace st::gfx