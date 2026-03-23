#pragma once

#include "RHI/Texture.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"
#include <array>

namespace alm::rhi::dx12
{
	class Texture : public ITexture
	{
	public:

		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, Device* device, const std::string& debugName);
		virtual ~Texture() override = default;

		const TextureDesc& GetDesc() const override;

		rhi::TextureSampledView GetSampledView() override { return m_SampledView; }
		rhi::TextureStorageView GetStorageView() override { return m_StorageView; }

		void Swap(ITexture& other) override;

		ResourceType GetResourceType() const override { return ResourceType::Texture; }
		NativeResource GetNativeResource() override { return m_D3d12Resource.Get(); }

	protected:

		void Release(Device* device) override;

	private:

		TextureDesc m_Desc;

		TextureSampledView m_SampledView;
		TextureStorageView m_StorageView;

		ComPtr<ID3D12Resource> m_D3d12Resource;
	};
} // namespace st::rhi::dx12