#include "RHI/RHI_PCH.h"
#include "RHI/dx12/FrameBuffer.h"
#include "RHI/dx12/GpuDevice.h"

void st::rhi::dx12::Framebuffer::Release(Device* device)
{
    auto* gpuDevice = checked_cast<GpuDevice*>(device);

    rtvTextures.clear();

    auto rtvDescriptorHeap = gpuDevice->GetRenderTargetViewHeap();
    for (DescriptorIndex rtvIdx : RTVs)
    {
        rtvDescriptorHeap->ReleaseDescriptor(rtvIdx);
    }
    RTVs.clear();

    if (DSV != c_InvalidDescriptorIndex)
    {
        auto dsvDescriptorHeap = gpuDevice->GetDepthStencilViewHeap();
        dsvDescriptorHeap->ReleaseDescriptor(DSV);
        DSV = c_InvalidDescriptorIndex;
    }
};