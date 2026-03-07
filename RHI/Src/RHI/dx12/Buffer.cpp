#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/dx12/Buffer.h"
#include "RHI/dx12/GpuDevice.h"

st::rhi::dx12::Buffer::Buffer(const BufferDesc& desc, ID3D12Resource* buffer, Device* device, const std::string& debugName) :
    IBuffer{ device, debugName },
    m_Desc{ desc }, 
    m_mapAddr{ nullptr },
    m_Resource { buffer }
{
    if (desc.memoryAccess == MemoryAccess::Upload || desc.memoryAccess == MemoryAccess::Readback)
    {
        // always mapped
        m_Resource->Map(0, nullptr, (void**)&m_mapAddr);
    }

    if (has_any_flag(desc.shaderUsage, BufferShaderUsage::Uniform))
    {
        m_UniformView = device->CreateBufferUniformView(this);
    }
    if (has_any_flag(desc.shaderUsage, BufferShaderUsage::ReadOnly))
    {
        m_ReadOnlyView = device->CreateBufferReadOnlyView(this);
    }
    if (has_any_flag(desc.shaderUsage, BufferShaderUsage::ReadWrite))
    {
        m_ReadWriteView = device->CreateBufferReadWriteView(this);
    }
}

st::rhi::dx12::Buffer::~Buffer()
{
    if (m_mapAddr)
        m_Resource->Unmap(0, nullptr);
}

void st::rhi::dx12::Buffer::Release(Device* device)
{
    if (m_UniformView.IsValid())
        device->ReleaseBufferUniformView(m_UniformView, true);
    if (m_ReadOnlyView.IsValid())
        device->ReleaseBufferReadOnlyView(m_ReadOnlyView, true);
    if (m_ReadWriteView.IsValid())
        device->ReleaseBufferReadWriteView(m_ReadWriteView, true);

    if (m_mapAddr)
        m_Resource->Unmap(0, nullptr);
    m_mapAddr = nullptr;

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

void st::rhi::dx12::Buffer::Swap(IBuffer& other)
{
    auto* otherBuffer = st::checked_cast<Buffer*>(&other);

    std::swap(m_Desc, otherBuffer->m_Desc);
    std::swap(m_mapAddr, otherBuffer->m_mapAddr);
    std::swap(m_UniformView, otherBuffer->m_UniformView);
    std::swap(m_ReadOnlyView, otherBuffer->m_ReadOnlyView);
    std::swap(m_ReadWriteView, otherBuffer->m_ReadWriteView);
    std::swap(m_Resource, otherBuffer->m_Resource);
}
