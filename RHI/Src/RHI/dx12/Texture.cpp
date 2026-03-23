#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/dx12/Texture.h"
#include "RHI/dx12/GpuDevice.h"

alm::rhi::dx12::Texture::Texture(const alm::rhi::TextureDesc& desc, ComPtr<ID3D12Resource> resource, Device* device, const std::string& debugName)
    : ITexture{ device, debugName }
    , m_Desc{ desc }
	, m_D3d12Resource{ resource }
{
    GpuDevice* gpuDevice = checked_cast<GpuDevice*>(device);
    if (has_any_flag(desc.shaderUsage, TextureShaderUsage::Sampled))
    {
        m_SampledView = device->CreateTextureSampledView(this);
    }
    if (has_any_flag(desc.shaderUsage, TextureShaderUsage::Storage))
    {
        m_StorageView = device->CreateTextureStorageView(this);
    }
}

const alm::rhi::TextureDesc& alm::rhi::dx12::Texture::GetDesc() const
{
	return m_Desc;
}

void alm::rhi::dx12::Texture::Swap(ITexture& other)
{
    auto* otherTex = alm::checked_cast<Texture*>(&other);

    std::swap(m_Desc, otherTex->m_Desc);
    std::swap(m_SampledView, otherTex->m_SampledView);
    std::swap(m_StorageView, otherTex->m_StorageView);
    std::swap(m_D3d12Resource, otherTex->m_D3d12Resource);
}

void alm::rhi::dx12::Texture::Release(Device* device)
{
    if (m_SampledView.IsValid())
    {
        device->ReleaseTextureSampledView(m_SampledView, true);
    }
    if (m_StorageView.IsValid())
    {
        device->ReleaseTextureStorageView(m_StorageView, true);
    }

    m_D3d12Resource.Release();
}