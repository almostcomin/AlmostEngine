#pragma once

#include "RenderAPI/CommandList.h"
#include "RenderAPI/Texture.h"
#include <directx/d3d12.h>

namespace st::rapi::dx12
{
	class CommandList : public ICommandList
	{
	public:

		CommandList(ID3D12GraphicsCommandList* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type, ID3D12Device* device) :
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
		
		void Discard(IBuffer* buffer) override;
		void Discard(ITexture* texture, int mipLevel, int arraySlice) override;

		void BeginMarker(const char* str) override;
		void EndMarker() override;

		QueueType GetType() const override { return m_Type; }

		NativeResource GetNativeResource() override { return m_D3d12Commandlist.Get(); }

	private:

		ComPtr<ID3D12GraphicsCommandList> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		
		QueueType m_Type;

		ID3D12Device* m_Device;
	};
}