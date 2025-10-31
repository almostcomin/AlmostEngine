#include "RenderAPI/dx12/Buffer.h"
#include <cassert>

void* st::rapi::dx12::Buffer::Map(uint64_t bufferStart, size_t size)
{
    if (size == 0)
        size = m_Desc.sizeBytes - bufferStart;
    
    assert(bufferStart + size <= m_Desc.sizeBytes);

    D3D12_RANGE range = { bufferStart, bufferStart + size };

    void* data = nullptr;
    HRESULT hr = m_Resource->Map(0, &range, &data);
    assert(SUCCEEDED(hr));
    
    return data;
}