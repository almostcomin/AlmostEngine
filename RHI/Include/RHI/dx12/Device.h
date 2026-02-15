#pragma once

#include "RHI/dx12/d3d12_headers.h"
#include <cstdint>
#include <memory>
#include "RHI/Device.h"

namespace st::rhi::dx12
{
    struct DeviceDesc
    {
        ID3D12Device* pDevice = nullptr;
        ID3D12CommandQueue* pGraphicsCommandQueue = nullptr;
        ID3D12CommandQueue* pComputeCommandQueue = nullptr;
        ID3D12CommandQueue* pCopyCommandQueue = nullptr;

        uint32_t renderTargetViewHeapSize = 256;
        uint32_t depthStencilViewHeapSize = 32;
        uint32_t shaderResourceViewHeapSize = UINT_MAX; // Platform defined max
        uint32_t samplerHeapSize = 16; // Though static ones should be enough
        uint32_t maxTimerQueries = 256;

        uint32_t swapChainFrames = 3;        
    };

    std::unique_ptr<st::rhi::Device> CreateDevice(const DeviceDesc& desc);
    void CheckDRED(ID3D12Device* pDevice);

} // namespace st::rhi::dx12