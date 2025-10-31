#pragma once

#include "RenderAPI/Buffer.h"
#include <wrl/client.h>
#include <d3d12.h>

namespace st::rapi::dx12
{
	class Buffer : public st::rapi::IBuffer
	{
	public:

		Buffer(const BufferDesc& desc, ID3D12Resource* buffer) : m_Desc{ desc }, m_Resource{ buffer }
		{}

		const BufferDesc& GetDesc() const override { return m_Desc; }
		GpuVirtualAddress GetGpuVirtualAddress() const override { return {}; }

		void* Map(uint64_t bufferStart = 0, size_t size = 0) override;

		NativeResource GetNativeResource() override { return m_Resource.Get(); }

	private:

		BufferDesc m_Desc;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
	};

}  // namespace st::rapi