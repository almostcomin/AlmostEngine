#pragma once

#include "RenderAPI/Buffer.h"
#include <wrl/client.h>
#include <directx/d3d12.h>

namespace st::rapi::dx12
{
	class Buffer : public st::rapi::IBuffer
	{
	public:

		Buffer(const BufferDesc& desc, ID3D12Resource* buffer);
		~Buffer() override;

		const BufferDesc& GetDesc() const override { return m_Desc; }
		GpuVirtualAddress GetGpuVirtualAddress() const override { return {}; }

		void* Map(uint64_t bufferStart = 0, size_t size = 0) override;
		void Unmap(uint64_t bufferStart = 0, size_t size = 0) override;

	private:

		BufferDesc m_Desc;
		char* m_mapAddr;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
	};

}  // namespace st::rapi