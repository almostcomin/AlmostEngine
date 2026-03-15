#pragma once

#include "Gfx/MaterialDomain.h"
#include "RHI/PipelineState.h"
#include "RHI/FrameBuffer.h"

namespace st::rhi
{
	struct GraphicsPipelineStateDesc;
	struct FramebufferInfo;
	class Device;
}

namespace st::gfx
{

class RenderContext
{
public:

	RenderContext();
	
	void Init(const st::rhi::GraphicsPipelineStateDesc& baseDesc, const st::rhi::FramebufferInfo& fbInfo,
		std::string baseDebugName, st::rhi::Device* device);
	void Reset();

	void OnFramebufferChanged(const st::rhi::FramebufferInfo& fbInfo);

	void AddDomain(MaterialDomain domain, rhi::ShaderHandle VS, rhi::ShaderHandle PS);

	rhi::IGraphicsPipelineState* Get(MaterialDomain domain, rhi::CullMode cullMode) const;

	const std::string& GetDebugName() const { return m_BaseDebugName; }

private:

	struct CullPSOs
	{
		rhi::GraphicsPipelineStateOwner PSO_BackCull;
		rhi::GraphicsPipelineStateOwner PSO_FrontCull;
		rhi::GraphicsPipelineStateOwner PSO_NoCull;
	};

	struct ShaderEntryType
	{
		rhi::ShaderHandle VS;
		rhi::ShaderHandle PS;
	};

	st::rhi::GraphicsPipelineStateDesc m_BasePSODesc;
	std::array<CullPSOs, (int)MaterialDomain::_Size> m_PSOs;
	st::rhi::FramebufferInfo m_FBInfo;
	std::string m_BaseDebugName;

	rhi::Device* m_Device;
};

} //namespace st::gfx