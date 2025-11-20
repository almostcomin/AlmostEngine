#pragma once

#include "RenderAPI/Texture.h"
#include "RenderAPI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"
#include "RenderAPI/Descriptors.h"
#include <array>

namespace st::rapi::dx12
{
	class GpuDevice;

	class Texture : public ITexture
	{
	public:

		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, GpuDevice* device);
		virtual ~Texture() override = default;

		const TextureDesc& GetDesc() const override;

		DescriptorIndex GetDescriptorIndex(DescriptorType type) override;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, const TextureSubresourceSet subresources) const;
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, const TextureSubresourceSet subresources) const;
		void CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources) const;
		void CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly = false) const;

		NativeResource GetNativeResource() override { return m_D3d12Resource.Get(); }

	private:

		TextureDesc m_Desc;
		ComPtr<ID3D12Resource> m_D3d12Resource;
		std::array<DescriptorIndex, (int)DescriptorType::_Size> m_DescriptorIndex;

		GpuDevice* m_Device;
	};
} // namespace st::rapi::dx12