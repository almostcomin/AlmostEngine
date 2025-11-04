#pragma once

#include <directx/d3d12.h>
#include <cstdint>
#include <vector>
#include <mutex>
#include "Core/ComPtr.h"

namespace st::rapi::dx12
{
    using DescriptorIndex = uint32_t;
    constexpr DescriptorIndex c_InvalidDescriptorIndex = ~0u;

    class DescriptorHeap
    {
    public:

        explicit DescriptorHeap(ID3D12Device* device);

        HRESULT AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool shaderVisible);

        DescriptorIndex AllocateDescriptors(uint32_t count);
        DescriptorIndex AllocateDescriptor();
        void ReleaseDescriptors(DescriptorIndex baseIndex, uint32_t count);
        void ReleaseDescriptor(DescriptorIndex index);
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorIndex index);
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(DescriptorIndex index);
        [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const;
        //[[nodiscard]] ID3D12DescriptorHeap* GetShaderVisibleHeap() const;

    private:

        HRESULT Grow(uint32_t minRequiredSize);

        ComPtr<ID3D12DescriptorHeap> m_Heap;
        bool m_ShaderVisible = false;
        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandle = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_StartGpuHandleShaderVisible = { 0 };
        uint32_t m_Stride = 0;
        uint32_t m_NumDescriptors = 0;
        std::vector<bool> m_AllocatedDescriptors;
        DescriptorIndex m_SearchStart = 0;
        uint32_t m_NumAllocatedDescriptors = 0;
        std::mutex m_Mutex;

        ComPtr<ID3D12Device> m_Device;
    };

} // namespace st::rapi::dx12