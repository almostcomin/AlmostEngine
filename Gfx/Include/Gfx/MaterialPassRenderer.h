#pragma once

#include "Gfx/MaterialDomain.h"
#include "Gfx/GpuSceneBuffers.h"
#include "RHI/PipelineState.h"
#include "RHI/Framebuffer.h"

namespace alm::rhi
{
	struct GraphicsPipelineStateDesc;
	struct FramebufferInfo;
	class Device;
}

namespace alm::gfx
{
	struct RenderSet;
}

namespace alm::gfx
{

class MaterialPassRenderer
{
public:

	struct IndirectDrawParams
	{
		rhi::IBuffer* ArgsBuffer = nullptr;
		const GpuSceneBuffers::BucketInfoArray* Buckets = nullptr;
		const std::span<const GpuSceneBuffers::TransientRecord> Transients;
	};

public:

	MaterialPassRenderer();
	
	void Init(const alm::rhi::GraphicsPipelineStateDesc& baseDesc, const alm::rhi::FramebufferInfo& fbInfo,
		std::string baseDebugName, alm::rhi::Device* device);
	void Reset();

	void OnFramebufferChanged(const alm::rhi::FramebufferInfo& fbInfo);

	void AddDomain(MaterialDomain domain, rhi::ShaderHandle VS, rhi::ShaderHandle PS);

	rhi::IGraphicsPipelineState* GetPSO(MaterialDomain domain, rhi::CullMode cullMode) const;

	void DrawRenderSetInstanced(const alm::gfx::RenderSet& renderSet, alm::rhi::ICommandList* commandList) const;
	void DrawIndirect(const IndirectDrawParams& params, alm::rhi::ICommandList* commandList) const;

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

	alm::rhi::GraphicsPipelineStateDesc m_BasePSODesc;
	std::array<CullPSOs, (int)MaterialDomain::_Size> m_PSOs;
	std::array<bool, (int)MaterialDomain::_Size> m_ValidDomains;
	alm::rhi::FramebufferInfo m_FBInfo;

	alm::rhi::CommandSignatureOwner m_CommandSignature;

	std::string m_BaseDebugName;
	rhi::Device* m_Device;
};

} //namespace st::gfx