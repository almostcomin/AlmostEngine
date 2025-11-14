#pragma once

#include "RenderAPI/Texture.h"
#include "RenderAPI/dx12/d3d12_headers.h"

namespace st::rapi::dx12
{
	class Texture : public ITexture
	{
		friend class GpuDevice;

	public:

		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, ID3D12Device* device);
		virtual ~Texture() override = default;

		const TextureDesc& GetDesc() const override;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const;
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const;
		void CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources) const;
		void CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly = false) const;

		NativeResource GetNativeResource() override { return m_D3d12Resource.Get(); }

	private:

		TextureDesc m_Desc;
		ComPtr<ID3D12Resource> m_D3d12Resource;
		ID3D12Device* m_D3d12Device;
	};
} // namespace st::rapi::dx12