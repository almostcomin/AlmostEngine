#pragma once

#include "RenderAPI/dx12/d3d12_headers.h"
#include "RenderAPI/Buffer.h"
#include "Core/ComPtr.h"
#include <array>

namespace st::rapi::dx12
{
	class GpuDevice;

	class Buffer : public st::rapi::IBuffer
	{
	public:

		Buffer(const BufferDesc& desc, ID3D12Resource* buffer, GpuDevice* device);
		~Buffer() override;

		const BufferDesc& GetDesc() const override { return m_Desc; }
		GpuVirtualAddress GetGpuVirtualAddress() const override { return {}; }

		void* Map(uint64_t bufferStart = 0, size_t size = 0) override;
		void Unmap(uint64_t bufferStart = 0, size_t size = 0) override;

		DescriptorIndex GetDescriptorIndex(DescriptorType type) override;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes);
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes);

	private:

		BufferDesc m_Desc;
		char* m_mapAddr;
		std::array<DescriptorIndex, (int)DescriptorType::_Size> m_DescriptorIndex;
		ComPtr<ID3D12Resource> m_Resource;

		GpuDevice* m_Device;
	};

}  // namespace st::rapi