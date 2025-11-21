#include "RenderAPI/dx12/DescriptorHeap.h"
#include "Core/Log.h"
#include "Core/Util.h"
#include <cassert>
#include <algorithm>

st::rapi::dx12::DescriptorHeap::DescriptorHeap(ID3D12Device* device)
	: m_Device{ device }
{}

HRESULT st::rapi::dx12::DescriptorHeap::AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool shaderVisible)
{
    m_Heap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = heapType;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));

    if (FAILED(hr))
        return hr;

    m_ShaderVisible = shaderVisible;
    m_NumDescriptors = heapDesc.NumDescriptors;
    m_HeapType = heapDesc.Type;
    m_StartCpuHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_StartGpuHandleShaderVisible = m_Heap->GetGPUDescriptorHandleForHeapStart();
    m_Stride = m_Device->GetDescriptorHandleIncrementSize(heapDesc.Type);
    m_AllocatedDescriptors.resize(m_NumDescriptors, false);

    return S_OK;
}

st::rapi::DescriptorIndex st::rapi::dx12::DescriptorHeap::DescriptorHeap::AllocateDescriptors(uint32_t count)
{
    std::lock_guard lockGuard(m_Mutex);

    DescriptorIndex foundIndex = 0;
    uint32_t freeCount = 0;
    bool found = false;

    // Find a contiguous range of 'count' indices for which m_AllocatedDescriptors[index] is false

    for (DescriptorIndex index = m_SearchStart; index < m_NumDescriptors; index++)
    {
        if (m_AllocatedDescriptors[index])
            freeCount = 0;
        else
            freeCount += 1;

        if (freeCount >= count)
        {
            foundIndex = index - count + 1;
            found = true;
            break;
        }
    }

    if (!found)
    {
        foundIndex = m_NumDescriptors;

        if (FAILED(Grow(m_NumDescriptors + count)))
        {
            LOG_ERROR("Failed to grow a descriptor heap!");
            return c_InvalidDescriptorIndex;
        }
    }

    for (DescriptorIndex index = foundIndex; index < foundIndex + count; index++)
    {
        m_AllocatedDescriptors[index] = true;
    }

    m_NumAllocatedDescriptors += count;

    m_SearchStart = foundIndex + count;
    return foundIndex;
}

st::rapi::DescriptorIndex st::rapi::dx12::DescriptorHeap::AllocateDescriptor()
{
    return AllocateDescriptors(1);
}

void st::rapi::dx12::DescriptorHeap::ReleaseDescriptors(DescriptorIndex baseIndex, uint32_t count)
{
    std::lock_guard lockGuard(m_Mutex);

    if (count == 0)
        return;

    for (DescriptorIndex index = baseIndex; index < baseIndex + count; index++)
    {
#ifdef _DEBUG
        if (!m_AllocatedDescriptors[index])
        {
            LOG_ERROR("Attempted to release an un-allocated descriptor");
        }
#endif

        m_AllocatedDescriptors[index] = false;
    }

    m_NumAllocatedDescriptors -= count;

    if (m_SearchStart > baseIndex)
        m_SearchStart = baseIndex;
}

void st::rapi::dx12::DescriptorHeap::ReleaseDescriptor(DescriptorIndex index)
{
    ReleaseDescriptors(index, 1);
}

D3D12_CPU_DESCRIPTOR_HANDLE st::rapi::dx12::DescriptorHeap::GetCpuHandle(DescriptorIndex index)
{
    if (index == c_InvalidDescriptorIndex)
        return { 0u };

    assert(index < m_NumDescriptors && "Descriptor index out of bounds");

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_StartCpuHandle;
    handle.ptr += index * m_Stride;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE st::rapi::dx12::DescriptorHeap::GetGpuHandle(DescriptorIndex index)
{
    assert(m_ShaderVisible);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_StartGpuHandleShaderVisible;
    handle.ptr += index * m_Stride;
    return handle;
}

ID3D12DescriptorHeap* st::rapi::dx12::DescriptorHeap::GetHeap() const
{
    return m_Heap.Get();
}

/*
void st::rapi::dx12::DescriptorHeap::CopyToShaderVisibleHeap(DescriptorIndex index, uint32_t count)
{
    m_Context.device->CopyDescriptorsSimple(count, getCpuHandleShaderVisible(index), getCpuHandle(index), m_HeapType);
}
*/

HRESULT st::rapi::dx12::DescriptorHeap::Grow(uint32_t minRequiredSize)
{
    uint32_t oldSize = m_NumDescriptors;
    uint32_t newSize = NextPowerOf2(minRequiredSize);

    if (m_ShaderVisible)
    {
        uint32_t maxSize = (m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            ? D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 // Not a power of 2!
            : D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

        newSize = (std::min)(newSize, maxSize);

        if (newSize < minRequiredSize)
            return E_OUTOFMEMORY;
    }

    ComPtr<ID3D12DescriptorHeap> oldHeap = m_Heap;

    HRESULT hr = AllocateResources(m_HeapType, newSize, m_ShaderVisible);

    if (FAILED(hr))
        return hr;

    m_Device->CopyDescriptorsSimple(oldSize, m_StartCpuHandle, oldHeap->GetCPUDescriptorHandleForHeapStart(), m_HeapType);

    return S_OK;
}