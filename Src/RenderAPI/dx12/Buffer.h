#pragma once

#include "RenderAPI/Buffer.h"
#include <wrl/client.h>
#include <d3d12.h>

namespace st::rapi::dx12
{
	class Buffer : public st::rapi::IBuffer
	{
	public:

		Buffer(const BufferDesc& desc, ID3D12Resource* buffer);

		const BufferDesc& GetDesc() const override { return m_Desc; }
		GpuVirtualAddress GetGpuVirtualAddress() const override { return {}; }

	private:

		BufferDesc m_Desc;
	};

}  // namespace st::rapi