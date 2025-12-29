#pragma once

#include "RHI/CommandList.h"
#include "RHI/Texture.h"
#include "RHI/Framebuffer.h"
#include "RHI/PipelineState.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rhi::dx12
{
	class CommandList : public ICommandList
	{
	public:

		using NativeCommandListType = ID3D12GraphicsCommandList4;

		CommandList(NativeCommandListType* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type, Device* device, const std::string& debugName) :
			ICommandList(device, debugName),
			m_D3d12Commandlist{ d3d12CommandList },
			m_D3d12CommandAllocator{ commandAllocator },
			m_Type{ type }
		{}

		void Open() override;
		void Close() override;

		void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) override;
		void WriteTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) override;

		void PushBarriers(std::span<const Barrier> barriers) override;
		void PushBarrier(const Barrier& barrier) override;
		
		void SetPipelineState(IGraphicsPipelineState* pso) override;

		void SetViewport(const rhi::ViewportState& vp) override;

		void SetStencilRef(uint8_t value);

		void SetBlendFactor(const float4& value);

		void PushConstants(const void* data, size_t sizeBytes, size_t offsetBytes) override;

		void BeginRenderPass(rhi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, const RenderPassOp& dsvRenderPassOp, RenderPassFlags flags) override;
		void EndRenderPass() override;

		void Draw(uint32_t vertexCount) override;
		void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount) override;

		void Discard(IBuffer* buffer) override;
		void Discard(ITexture* texture, int mipLevel, int arraySlice) override;

		void BeginMarker(const char* str) override;
		void EndMarker() override;

		QueueType GetType() const override { return m_Type; }

		ResourceType GetResourceType() const override { return ResourceType::CommandList; }
		NativeResource GetNativeResource() override { return m_D3d12Commandlist.Get(); }

	private:

		void Release(Device* device) override;

		ComPtr<NativeCommandListType> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		
		QueueType m_Type;

		st::rhi::GraphicsPipelineStateHandle m_CurrentPSO;
		st::rhi::FramebufferHandle m_CurrentFB;
	};
}