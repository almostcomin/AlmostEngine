#pragma once

#include "RHI/CommandList.h"
#include "RHI/Texture.h"
#include "RHI/Framebuffer.h"
#include "RHI/PipelineState.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace alm::rhi::dx12
{
	class Queue;

	class CommandList : public ICommandList
	{
		friend class GpuDevice;

	public:

		using NativeCommandListType = ID3D12GraphicsCommandList4;

		CommandList(NativeCommandListType* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type, Device* device, const std::string& debugName) :
			ICommandList(device, debugName),
			m_D3d12Commandlist{ d3d12CommandList },
			m_D3d12CommandAllocator{ commandAllocator },
			m_Type{ type },
			m_DrawCalls{ 0 },
			m_PrimitiveCount{ 0 }
		{}

		void Open() override;
		void Close() override;

		void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) override;
		void WriteTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) override;

		void CopyTextureToTexture(ITexture* dstTexture, ITexture* srcTexture) override;
		void CopyTextureToTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& dstSubresources,
			ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources) override;

		void CopyBufferToBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, uint64_t size) override;

		void CopyTextureToBuffer(IBuffer* dstBuffer, ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources) override;

		void PushBarriers(std::span<const Barrier> barriers) override;
		
		void SetPipelineState(IGraphicsPipelineState* pso) override;
		void SetPipelineState(IComputePipelineState* pso) override;

		void SetViewport(const rhi::ViewportState& vp) override;

		void SetStencilRef(uint8_t value);

		void SetBlendFactor(const float4& value);

		void PushConstants(uint32_t slot, const void* data, size_t sizeBytes, size_t offsetBytes, bool isCompute) override;

		void BeginRenderPass(rhi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, 
			const RenderPassOp& depthRenderPassOp, const RenderPassOp& stencilRenderPassOp, RenderPassFlags flags) override;
		void EndRenderPass() override;

		void Draw(uint32_t vertexCount) override;
		void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertex) override;

		void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;

		void Discard(IBuffer* buffer) override;
		void Discard(ITexture* texture, int mipLevel, int arraySlice) override;

		void BeginMarker(const char* str) override;
		void EndMarker() override;

		void BeginTimerQuery(ITimerQuery* query) override;
		void EndTimerQuery(ITimerQuery* query) override;

		void OnExecuted(Queue& queue);

		QueueType GetType() const override { return m_Type; }

		ResourceType GetResourceType() const override { return ResourceType::CommandList; }
		NativeResource GetNativeResource() override { return m_D3d12Commandlist.Get(); }

	private:

		void Release(Device* device) override;

		ComPtr<NativeCommandListType> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		
		QueueType m_Type;

		alm::rhi::GraphicsPipelineStateHandle m_CurrentGraphicsPSO;
		alm::rhi::ComputePipelineStateHandle m_CurrentComputePSO;
		alm::rhi::FramebufferHandle m_CurrentFB;

		std::vector<TimerQueryHandle> m_BeginTimerQueries;
		std::vector<TimerQueryHandle> m_EndTimerQueries;

		uint32_t m_DrawCalls;
		uint32_t m_DispatchCalls;
		uint32_t m_PrimitiveCount;
	};
}