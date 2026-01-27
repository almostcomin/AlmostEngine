#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/dx12/Buffer.h"
#include "RHI/dx12/GpuDevice.h"

st::rhi::dx12::Buffer::Buffer(const BufferDesc& desc, ID3D12Resource* buffer, Device* device, const std::string& debugName) :
    IBuffer{ device, debugName },
    m_Desc{ desc }, 
    m_mapAddr{ nullptr },
    m_ShaderViews{ c_InvalidDescriptorIndex, c_InvalidDescriptorIndex, c_InvalidDescriptorIndex },
    m_Resource { buffer }
{
    if (desc.memoryAccess == MemoryAccess::Upload)
    {
        // always mapped
        m_Resource->Map(0, nullptr, (void**)&m_mapAddr);
    }
    
    GpuDevice* gpuDevice = checked_cast<GpuDevice*>(device);
    if (has_flag(desc.shaderUsage, BufferShaderUsage::ConstantBuffer))
    {
        auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
        m_ShaderViews[(int)BufferShaderView::ConstantBuffer] = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(m_ShaderViews[(int)BufferShaderView::ConstantBuffer]);
        CreateCBV(descriptorHandle, 0, m_Desc.sizeBytes);
    }
    if (has_flag(desc.shaderUsage, BufferShaderUsage::ShaderResource))
    {
        auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
        m_ShaderViews[(int)BufferShaderView::ShaderResource] = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(m_ShaderViews[(int)BufferShaderView::ShaderResource]);
        CreateSRV(descriptorHandle, m_Desc.format, 0, m_Desc.sizeBytes);
    }
    if (has_flag(desc.shaderUsage, BufferShaderUsage::UnorderedAccess))
    {
        auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
        m_ShaderViews[(int)BufferShaderView::UnorderedAccess] = descriptorHeap->AllocateDescriptor();
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCpuHandle(m_ShaderViews[(int)BufferShaderView::UnorderedAccess]);
        CreateUAV(descriptorHandle, m_Desc.format, 0, m_Desc.sizeBytes);
    }
}

st::rhi::dx12::Buffer::~Buffer()
{
    if (m_mapAddr)
        m_Resource->Unmap(0, nullptr);
}

void st::rhi::dx12::Buffer::Release(Device* device)
{
    auto* gpuDevice = checked_cast<GpuDevice*>(device);

    if (m_mapAddr)
        m_Resource->Unmap(0, nullptr);
    m_mapAddr = nullptr;

    for (size_t i = 0; i < m_ShaderViews.size(); ++i)
    {
        if (m_ShaderViews[i] != c_InvalidDescriptorIndex)
        {
            auto descriptorHeap = gpuDevice->GetShaderResourceViewHeap();
            descriptorHeap->ReleaseDescriptor(m_ShaderViews[i]);
            m_ShaderViews[i] = c_InvalidDescriptorIndex;
        }
    }

    m_Resource.Release();
}

void* st::rhi::dx12::Buffer::Map(uint64_t bufferStart, [[maybe_unused]] size_t size)
{
#ifdef _DEBUG
    if (size == 0)
        size = m_Desc.sizeBytes - bufferStart;    
    assert(bufferStart + size <= m_Desc.sizeBytes);
#endif
    if (!m_mapAddr)
    {
        LOG_ERROR("Trying to map a un-mappable buffer, name '{}'", GetDebugName());
        return nullptr;
    }

    return m_mapAddr + bufferStart;
}

void st::rhi::dx12::Buffer::Unmap(uint64_t /*bufferStart*/, size_t /*size*/)
{
    // no-op
}

st::rhi::DescriptorIndex st::rhi::dx12::Buffer::GetShaderViewIndex(BufferShaderView type)
{
    return m_ShaderViews[(int)type];
}

void st::rhi::dx12::Buffer::Swap(IBuffer& other)
{
    auto* otherBuffer = st::checked_cast<Buffer*>(&other);

    std::swap(m_Desc, otherBuffer->m_Desc);
    std::swap(m_mapAddr, otherBuffer->m_mapAddr);
    std::swap(m_ShaderViews, otherBuffer->m_ShaderViews);
    std::swap(m_Resource, otherBuffer->m_Resource);
}

void st::rhi::dx12::Buffer::CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, size_t sizeBytes)
{
    assert(IsAligned(offsetBytes, (uint32_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
    assert(IsAligned(sizeBytes, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = m_Resource->GetGPUVirtualAddress() + offsetBytes;
    desc.SizeInBytes = sizeBytes;

    GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
    gpuDevice->GetNativeDevice()->CreateConstantBufferView(
        &desc, descriptor);
}

void st::rhi::dx12::Buffer::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rhi::Format format, uint32_t offsetBytes, size_t sizeBytes)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // Structured buffer
    if (format == Format::UNKNOWN)
    {
        if (m_Desc.stride != 0)
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
    // Typed buffer
    else
    {
        uint32_t stride = GetFormatInfo(format).bytesPerBlock;
        viewDesc.Format = GetDxgiFormatMapping(format).srvFormat;
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Buffer.FirstElement = offsetBytes / stride;
        viewDesc.Buffer.NumElements = std::min(sizeBytes, m_Desc.sizeBytes) / stride;
    }

    GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
    gpuDevice->GetNativeDevice()->CreateShaderResourceView(
        m_Resource.Get(), &viewDesc, descriptor);
}

void st::rhi::dx12::Buffer::CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, st::rhi::Format format, uint32_t offsetBytes, size_t sizeBytes)
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

    GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
    gpuDevice->GetNativeDevice()->CreateUnorderedAccessView(
        m_Resource.Get(), nullptr, &viewDesc, descriptor);
}