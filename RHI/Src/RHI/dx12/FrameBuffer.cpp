#include "RHI/RHI_PCH.h"
#include "RHI/dx12/FrameBuffer.h"
#include "RHI/dx12/GpuDevice.h"

alm::rhi::TextureColorTargetView alm::rhi::dx12::Framebuffer::GetColorTargetView(uint32_t idx) const
{
    if (idx > RTVs.size())
        return {};
    return TextureColorTargetView{ RTVs[idx] };
}

void alm::rhi::dx12::Framebuffer::Release(Device* device)
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