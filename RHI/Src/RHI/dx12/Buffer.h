#pragma once

#include "RHI/dx12/d3d12_headers.h"
#include "RHI/Buffer.h"
#include "Core/ComPtr.h"

namespace st::rhi::dx12
{
	class Buffer : public st::rhi::IBuffer
	{
	public:

		Buffer(const BufferDesc& desc, ID3D12Resource* buffer, Device* device, const std::string& debugName);
		~Buffer() override;

		const BufferDesc& GetDesc() const override { return m_Desc; }
		GpuVirtualAddress GetGpuVirtualAddress() const override { return {}; }

		void* Map(uint64_t bufferStart = 0, size_t size = 0) override;
		void Unmap(uint64_t bufferStart = 0, size_t size = 0) override;

		BufferUniformView GetUniformView() override { return m_UniformView; }
		BufferReadOnlyView GetReadOnlyView() override { return m_ReadOnlyView; }
		BufferReadWriteView GetReadWriteView() override { return m_ReadWriteView; }

		void Swap(IBuffer& other) override;

		void CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, size_t sizeBytes);
		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rhi::Format format, uint32_t offsetBytes, size_t sizeBytes);
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rhi::Format format, uint32_t offsetBytes, size_t sizeBytes);

		ResourceType GetResourceType() const override { return ResourceType::Buffer; }
		NativeResource GetNativeResource() override { return m_Resource.Get(); }

	protected:

		void Release(Device* device) override;

	private:

		BufferDesc m_Desc;
		char* m_mapAddr;

		BufferUniformView m_UniformView;
		BufferReadOnlyView m_ReadOnlyView;
		BufferReadWriteView m_ReadWriteView;

		ComPtr<ID3D12Resource> m_Resource;
	};

}  // namespace st::rhi