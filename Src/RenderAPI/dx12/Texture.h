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

		ResourceType GetResourceType() const override { return ResourceType::Texture; }
		NativeResource GetNativeResource() override { return m_D3d12Resource.Get(); }
		const std::string& GetDebugName() override { return m_Desc.debugName; }

	protected:

		void Release(Device* device) override;

	private:

		TextureDesc m_Desc;
		ComPtr<ID3D12Resource> m_D3d12Resource;
		DescriptorIndex m_SRV;
		DescriptorIndex m_UAV;

		GpuDevice* m_Device;
	};
} // namespace st::rapi::dx12