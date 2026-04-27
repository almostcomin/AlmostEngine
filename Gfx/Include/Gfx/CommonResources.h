#pragma once

#include "RHI/Shader.h"
#include "RHI/PipelineState.h"

namespace alm::rhi
{
	class Device;
	struct FramebufferInfo;
};

namespace alm::gfx
{
	class Mesh;
	class ShaderFactory;
	class DataUploader;
};

namespace alm::gfx
{

class CommonResources
{
public:

	CommonResources(ShaderFactory* shaderFactory, rhi::Device* device);
	~CommonResources();

	rhi::GraphicsPipelineStateOwner CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo);
	rhi::GraphicsPipelineStateOwner CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo, const rhi::ShaderHandle& PS, const std::string debugName);
	rhi::GraphicsPipelineStateOwner CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo, const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS,
		const std::string debugName);

	rhi::ComputePipelineStateHandle GetBlitComputePSO() { return m_BlitComputePSO.get_weak(); }

	rhi::ComputePipelineStateHandle GetClearBufferPSO() { return m_ClearBufferPSO.get_weak(); }
	rhi::ComputePipelineStateHandle GetClearTexturePSO() { return m_ClearTexturePSO.get_weak(); }

	rhi::ShaderHandle GetBlitVS() const { return m_BlitVS.get_weak(); }
	rhi::ShaderHandle GetBlitPS() const { return m_BlitPS.get_weak(); }

	// Generates a UV sphere centered at origin
	// stacks: horizontal subdivisions (latitude bands), >= 2
	// slices: vertical subdivisions (longitude segments), >= 3
	std::shared_ptr<alm::gfx::Mesh> CreateUVSphere(float radius, uint32_t stacks, uint32_t slices, alm::gfx::DataUploader* dataUploader, const std::string& name);

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

	rhi::ShaderOwner m_ClearTextureCS;
	rhi::ComputePipelineStateOwner m_ClearTexturePSO;
};

} // namespace st::gfx