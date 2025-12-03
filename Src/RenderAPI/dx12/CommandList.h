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

		CommandList(NativeCommandListType* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type, GpuDevice* device, std::string debugName) :
			m_D3d12Commandlist{ d3d12CommandList },
			m_D3d12CommandAllocator{ commandAllocator },
			m_Type{ type },
			m_Device{ device },
			m_DebugName{ debugName }
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

		void Draw(uint32_t vertexCount) override;

		void Discard(IBuffer* buffer) override;
		void Discard(ITexture* texture, int mipLevel, int arraySlice) override;

		void BeginMarker(const char* str) override;
		void EndMarker() override;

		QueueType GetType() const override { return m_Type; }

		ResourceType GetResourceType() const override { return ResourceType::CommandList; }
		NativeResource GetNativeResource() override { return m_D3d12Commandlist.Get(); }
		const std::string& GetDebugName() override { return m_DebugName; }

	private:

		void Release(Device* device) override;

		ComPtr<NativeCommandListType> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		
		QueueType m_Type;

		st::rapi::FramebufferHandle m_CurrentFB;

		std::string m_DebugName;

		GpuDevice* m_Device;
	};
}