#include "RenderAPI/dx12/Buffer.h"
#include <cassert>

st::rapi::dx12::Buffer::Buffer(const st::rapi::BufferDesc& desc, ID3D12Resource* buffer) : 
    m_Desc{ desc }, m_mapAddr{ nullptr }, m_Resource { buffer }
{
    if (desc.usage == BufferUsage::UploadBuffer || desc.usage == BufferUsage::ConstantBuffer)
    {
        // always mapped
        m_Resource->Map(0, nullptr, (void**)&m_mapAddr);
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
    
    return m_mapAddr + bufferStart;
}

void st::rapi::dx12::Buffer::Unmap(uint64_t /*bufferStart*/, size_t /*size*/)
{
    // no-op
}