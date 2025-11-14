#pragma once

#include "RenderAPI/dx12/d3d12_headers.h"
#include <cstdint>
#include <memory>
#include "RenderAPI/Device.h"

namespace st::rapi::dx12
{
    struct DeviceDesc
    {
        ID3D12Device* pDevice = nullptr;
        ID3D12CommandQueue* pGraphicsCommandQueue = nullptr;
        ID3D12CommandQueue* pComputeCommandQueue = nullptr;
        ID3D12CommandQueue* pCopyCommandQueue = nullptr;

        uint32_t renderTargetViewHeapSize = 1024;
        uint32_t depthStencilViewHeapSize = 1024;
        uint32_t shaderResourceViewHeapSize = 16384;
        uint32_t samplerHeapSize = 1024;
        uint32_t maxTimerQueries = 256;

        bool aftermathEnabled = false;

        // Enable logging the buffer lifetime to IMessageCallback
        // Useful for debugging resource lifetimes
        bool logBufferLifetime = false;
    };

    std::unique_ptr<st::rapi::Device> CreateDevice(const DeviceDesc& desc);

} // namespace st::rapi::dx12