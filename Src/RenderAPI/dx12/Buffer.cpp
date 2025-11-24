#include "RenderAPI/dx12/Buffer.h"
#include "RenderAPI/dx12/GpuDevice.h"
#include "RenderAPI/dx12/DxgiFormat.h"
#include <cassert>
#include <algorithm>

st::rapi::dx12::Buffer::Buffer(const st::rapi::BufferDesc& desc, ID3D12Resource* buffer, st::rapi::dx12::GpuDevice* device) :
    m_Desc{ desc }, 
    m_mapAddr{ nullptr }, 
    m_DescriptorIndex{ c_InvalidDescriptorIndex, c_InvalidDescriptorIndex },
    m_Resource { buffer },
    m_Device{ device }
{
    if (desc.memoryAccess == MemoryAccess::Upload)
    {
        // always mapped
        m_Resource->Map(0, nullptr, (void**)&m_mapAddr);
    }

    if (hasFlag(desc.shaderUsage, ShaderUsage::ShaderResource))
    {
        auto descriptorHeap = m_Device->GetShaderResourceViewHeap();
        DescriptorIndex index = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(index);
        CreateSRV(descriptorHandle, m_Desc.format, 0, m_Desc.sizeBytes);
        m_DescriptorIndex[(int)DescriptorType::SRV] = index;
    }
    if (hasFlag(desc.shaderUsage, ShaderUsage::UnorderedAccess))
    {
        auto descriptorHeap = m_Device->GetShaderResourceViewHeap();
        DescriptorIndex index = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(index);
        CreateUAV(descriptorHandle, m_Desc.format, 0, m_Desc.sizeBytes);
        m_DescriptorIndex[(int)DescriptorType::UAV] = index;
    }
}

st::rapi::dx12::Buffer::~Buffer()
{
    if (m_mapAddr)
        m_Resource->Unmap(0, nullptr);
}

void* st::rapi::dx12::Buffer::Map(uint64_t bufferStart, [[maybe_unused]] size_t size)
{
#ifdef _DEBUG
    if (size == 0)
        size = m_Desc.sizeBytes - bufferStart;    
    assert(bufferStart + size <= m_Desc.sizeBytes);
#endif
    assert(m_mapAddr != nullptr);

    return m_mapAddr + bufferStart;
}

void st::rapi::dx12::Buffer::Unmap(uint64_t /*bufferStart*/, size_t /*size*/)
{
    // no-op
}

st::rapi::DescriptorIndex st::rapi::dx12::Buffer::GetDescriptorIndex(DescriptorType type)
{
    return m_DescriptorIndex[(int)type];
}

void st::rapi::dx12::Buffer::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (format == Format::UNKNOWN)
    {
        if (m_Desc.stride != 0) // Structured buffer
        {
            assert(IsAligned(offsetBytes, m_Desc.stride));
            viewDesc.Format = DXGI_FORMAT_UNKNOWN;
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            viewDesc.Buffer.FirstElement = offsetBytes / m_Desc.stride;
            viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / m_Desc.stride;
            viewDesc.Buffer.StructureByteStride = m_Desc.stride;
        }
        else
        {
            viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            viewDesc.Buffer.FirstElement = offsetBytes / sizeof(uint32_t);
            viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / sizeof(uint32_t);
            viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        }
    }
    else
    {
        uint32_t stride = GetFormatInfo(format).bytesPerBlock;
        viewDesc.Format = GetDxgiFormatMapping(format).srvFormat;
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Buffer.FirstElement = offsetBytes / stride;
        viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / stride;
    }

    m_Device->GetNativeDevice()->CreateShaderResourceView(
        m_Resource.Get(), &viewDesc, descriptor);
}

void st::rapi::dx12::Buffer::CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rapi::Format format, uint32_t offsetBytes, size_t sizeBytes)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

    if (format == Format::UNKNOWN)
    {
        if (m_Desc.stride != 0) // Structured buffer
        {
            assert(IsAligned(offsetBytes, m_Desc.stride));
            viewDesc.Format = DXGI_FORMAT_UNKNOWN;
            viewDesc.Buffer.FirstElement = offsetBytes / m_Desc.stride;
            viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / m_Desc.stride;
            viewDesc.Buffer.StructureByteStride = m_Desc.stride;
        }
        else
        {
            viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            viewDesc.Buffer.FirstElement = offsetBytes / sizeof(uint32_t);
            viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / sizeof(uint32_t);
            viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        }
    }
    else
    {
        uint32_t stride = GetFormatInfo(format).bytesPerBlock;
        viewDesc.Format = GetDxgiFormatMapping(format).srvFormat;
        viewDesc.Buffer.FirstElement = offsetBytes / stride;
        viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / stride;
    }

    m_Device->GetNativeDevice()->CreateUnorderedAccessView(
        m_Resource.Get(), nullptr, &viewDesc, descriptor);
}