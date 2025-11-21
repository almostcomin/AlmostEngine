#pragma once

#include "RenderAPI/CommandList.h"
#include "RenderAPI/Texture.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rapi::dx12
{
	class GpuDevice;

	class CommandList : public ICommandList
	{
	public:

		using NativeCommandListType = ID3D12GraphicsCommandList4;

		CommandList(NativeCommandListType* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type, GpuDevice* device) :
			m_D3d12Commandlist{ d3d12CommandList },
			m_D3d12CommandAllocator{ commandAllocator },
			m_Type{ type },
			m_Device{ device }
		{}

		void Open() override;
		void Close() override;

		void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) override;
		void WriteTexture(ITexture* dstTexture, const rapi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) override;

		void PushBarriers(std::span<const Barrier> barriers) override;
		void PushBarrier(const Barrier& barrier) override;
		
		void SetPipelineState(IGraphicsPipelineState* pso) override;

		void SetViewport(const rapi::ViewportState& vp) override;

		void SetStencilRef(uint8_t value);

		void SetBlendFactor(const float4& value);

		void PushConstants(const void* data, size_t sizeBytes, size_t offsetBytes) override;

		void BeginRenderPass(rapi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, const RenderPassOp& dsvRenderPassOp, RenderPassFlags flags) override;
		void EndRenderPass() override;

		void DrawIndexed(uint32_t indexCount) override;

		void Discard(IBuffer* buffer) override;
		void Discard(ITexture* texture, int mipLevel, int arraySlice) override;

		void BeginMarker(const char* str) override;
		void EndMarker() override;

		QueueType GetType() const override { return m_Type; }

		NativeResource GetNativeResource() override { return m_D3d12Commandlist.Get(); }

	private:

		ComPtr<NativeCommandListType> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		
		QueueType m_Type;

		st::rapi::FramebufferHandle m_CurrentFB;

		GpuDevice* m_Device;
	};
}