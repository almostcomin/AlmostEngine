#pragma once

#include "RHI/Texture.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"
#include "RHI/Descriptors.h"
#include <array>

namespace st::rhi::dx12
{
	class Texture : public ITexture
	{
	public:

		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, Device* device, const std::string& debugName);
		virtual ~Texture() override = default;

		const TextureDesc& GetDesc() const override;
		DescriptorIndex GetShaderViewIndex(TextureShaderView type) override;

		void Swap(ITexture& other) override;

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, const TextureSubresourceSet subresources);
		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, const TextureSubresourceSet subresources);
		void CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources);
		void CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly = false);

		ResourceType GetResourceType() const override { return ResourceType::Texture; }
		NativeResource GetNativeResource() override { return m_D3d12Resource.Get(); }

	protected:

		void Release(Device* device) override;

	private:

		TextureDesc m_Desc;

		std::array<DescriptorIndex, (int)TextureShaderView::_Size> m_ShaderViews;

		ComPtr<ID3D12Resource> m_D3d12Resource;
	};
} // namespace st::rhi::dx12