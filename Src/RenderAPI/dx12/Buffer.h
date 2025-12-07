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

		DescriptorIndex GetShaderViewIndex(BufferShaderView type) override;

		void CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, size_t sizeBytes, GpuDevice* device);
		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes, GpuDevice* device);
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes, GpuDevice* device);

		ResourceType GetResourceType() const override { return ResourceType::Buffer; }
		NativeResource GetNativeResource() override { return m_Resource.Get(); }

		const std::string& GetDebugName() override { return m_Desc.debugName; }

	protected:

		void Release(Device* device) override;

	private:

		BufferDesc m_Desc;
		char* m_mapAddr;

		std::array<DescriptorIndex, (int)BufferShaderView::_Size> m_ShaderViews;

		ComPtr<ID3D12Resource> m_Resource;
	};

}  // namespace st::rapi