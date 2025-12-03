#include "RenderAPI/dx12/Texture.h"
#include "RenderAPI/dx12/DxgiFormat.h"
#include "RenderAPI/dx12/GpuDevice.h"

st::rapi::dx12::Texture::Texture(const st::rapi::TextureDesc& desc, ComPtr<ID3D12Resource> resource, GpuDevice* device)
	: m_Desc{ desc }
	, m_D3d12Resource{ resource }
    , m_SRV{ c_InvalidDescriptorIndex }
    , m_UAV{ c_InvalidDescriptorIndex }
    , m_Device{ device }
{
    if (hasFlag(desc.shaderUsage, ShaderUsage::ShaderResource))
    {
        auto descriptorHeap = m_Device->GetShaderResourceViewHeap();
        m_SRV = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(m_SRV);
        CreateSRV(descriptorHandle, desc.format, desc.dimension, AllSubresources);
    }
    if (hasFlag(desc.shaderUsage, ShaderUsage::UnorderedAccess))
    {
        auto descriptorHeap = m_Device->GetShaderResourceViewHeap();
        m_UAV = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(m_UAV);
        CreateUAV(descriptorHandle, desc.format, desc.dimension, AllSubresources);
    }
}

const st::rapi::TextureDesc& st::rapi::dx12::Texture::GetDesc() const
{
	return m_Desc;
}

st::rapi::DescriptorIndex st::rapi::dx12::Texture::GetDescriptorIndex(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::SRV:
        return m_SRV;
    case DescriptorType::UAV:
        return m_UAV;
    default:
        return c_InvalidDescriptorIndex;
    }
}

void st::rapi::dx12::Texture::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const
{
    subresources.Resolve(m_Desc);

    if (dimension == TextureDimension::Unknown)
        dimension = m_Desc.dimension;

    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};

    viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? m_Desc.format : format).srvFormat;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    uint32_t planeSlice = (viewDesc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT) ? 1 : 0;

    switch (dimension)
    {
    case TextureDimension::Texture1D:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
        viewDesc.Texture1D.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.Texture1D.MipLevels = subresources.numMipLevels;
        break;
    case TextureDimension::Texture1DArray:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture1DArray.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.Texture1DArray.MipLevels = subresources.numMipLevels;
        break;
    case TextureDimension::Texture2D:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.Texture2D.MipLevels = subresources.numMipLevels;
        viewDesc.Texture2D.PlaneSlice = planeSlice;
        break;
    case TextureDimension::Texture2DArray:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture2DArray.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.Texture2DArray.MipLevels = subresources.numMipLevels;
        viewDesc.Texture2DArray.PlaneSlice = planeSlice;
        break;
    case TextureDimension::TextureCube:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        viewDesc.TextureCube.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.TextureCube.MipLevels = subresources.numMipLevels;
        break;
    case TextureDimension::TextureCubeArray:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        viewDesc.TextureCubeArray.First2DArrayFace = subresources.baseArraySlice;
        viewDesc.TextureCubeArray.NumCubes = subresources.numArraySlices / 6;
        viewDesc.TextureCubeArray.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.TextureCubeArray.MipLevels = subresources.numMipLevels;
        break;
    case TextureDimension::Texture2DMS:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        break;
    case TextureDimension::Texture2DMSArray:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
        viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
        break;
    case TextureDimension::Texture3D:
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        viewDesc.Texture3D.MostDetailedMip = subresources.baseMipLevel;
        viewDesc.Texture3D.MipLevels = subresources.numMipLevels;
        break;
    case TextureDimension::Unknown:
    default:
        assert(0);
        return;
    }

    m_Device->GetNativeDevice()->CreateShaderResourceView(m_D3d12Resource.Get(), &viewDesc, descriptor);
}

void st::rapi::dx12::Texture::CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const
{
    subresources.Resolve(m_Desc);

    if (dimension == TextureDimension::Unknown)
        dimension = m_Desc.dimension;

    D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};

    viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? m_Desc.format : format).srvFormat;

    switch (dimension)
    {
    case TextureDimension::Texture1D:
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
        viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture1DArray:
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
        viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2D:
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DArray:
    case TextureDimension::TextureCube:
    case TextureDimension::TextureCubeArray:
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture3D:
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        viewDesc.Texture3D.FirstWSlice = 0;
        viewDesc.Texture3D.WSize = m_Desc.depth;
        viewDesc.Texture3D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DMS:
    case TextureDimension::Texture2DMSArray: {
        assert(0);
        return;
    }
    case TextureDimension::Unknown:
    default:
        assert(0);
        return;
    }

    m_Device->GetNativeDevice()->CreateUnorderedAccessView(m_D3d12Resource.Get(), nullptr, &viewDesc, descriptor);
}

void st::rapi::dx12::Texture::CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources) const
{
    subresources.Resolve(m_Desc);

    D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};

    viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? m_Desc.format : format).rtvFormat;

    switch (m_Desc.dimension)
    {
    case TextureDimension::Texture1D:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
        viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture1DArray:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
        viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2D:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DArray:
    case TextureDimension::TextureCube:
    case TextureDimension::TextureCubeArray:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DMS:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        break;
    case TextureDimension::Texture2DMSArray:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
        viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
        break;
    case TextureDimension::Texture3D:
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        viewDesc.Texture3D.FirstWSlice = subresources.baseArraySlice;
        viewDesc.Texture3D.WSize = subresources.numArraySlices;
        viewDesc.Texture3D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Unknown:
    default:
        assert(0);
        return;
    }

    m_Device->GetNativeDevice()->CreateRenderTargetView(m_D3d12Resource.Get(), &viewDesc, descriptor);
}

void st::rapi::dx12::Texture::CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly) const
{
    subresources.Resolve(m_Desc);

    D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};

    viewDesc.Format = GetDxgiFormatMapping(m_Desc.format).rtvFormat;

    if (isReadOnly)
    {
        viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        if (viewDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || viewDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
            viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
    }

    switch (m_Desc.dimension)
    {
    case TextureDimension::Texture1D:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
        viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture1DArray:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
        viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2D:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DArray:
    case TextureDimension::TextureCube:
    case TextureDimension::TextureCubeArray:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
        viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
        break;
    case TextureDimension::Texture2DMS:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        break;
    case TextureDimension::Texture2DMSArray:
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
        viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
        viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
        break;
    case TextureDimension::Texture3D: {
        assert(0);
        return;
    }
    case TextureDimension::Unknown:
    default:
        assert(0);
        return;
    }

    m_Device->GetNativeDevice()->CreateDepthStencilView(m_D3d12Resource.Get(), &viewDesc, descriptor);
}

void st::rapi::dx12::Texture::Release(Device* device)
{
    auto* gpuDevice = checked_cast<GpuDevice*>(device);

    if (m_SRV != c_InvalidDescriptorIndex)
    {
        auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
        descriptorHeap->ReleaseDescriptor(m_SRV);
        m_SRV = c_InvalidDescriptorIndex;
    }
    if (m_UAV != c_InvalidDescriptorIndex)
    {
        auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
        descriptorHeap->ReleaseDescriptor(m_UAV);
        m_UAV = c_InvalidDescriptorIndex;
    }

    m_D3d12Resource.Release();
}