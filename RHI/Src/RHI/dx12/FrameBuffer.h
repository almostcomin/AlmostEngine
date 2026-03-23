#pragma once

#include "RHI/Framebuffer.h"
#include "RHI/dx12/DescriptorHeap.h"

namespace alm::rhi::dx12
{

class Framebuffer : public IFramebuffer
{
    friend class GpuDevice;

public:

    FramebufferDesc desc;
    FramebufferInfo framebufferInfo;

    static_vector<TextureHandle, c_MaxRenderTargets> rtvTextures;
    static_vector<DescriptorIndex, c_MaxRenderTargets> RTVs;
    TextureHandle dsvTexture;
    DescriptorIndex DSV = c_InvalidDescriptorIndex;
    uint32_t rtWidth = 0;
    uint32_t rtHeight = 0;

    const FramebufferDesc& GetDesc() const override { return desc; }
    const FramebufferInfo& GetFramebufferInfo() const override { return framebufferInfo; }

    TextureHandle GetBackBuffer(uint32_t idx) override { assert(idx < c_MaxRenderTargets); return rtvTextures[idx]; }
    TextureHandle GetDepthStencil() override { return dsvTexture; }

    ResourceType GetResourceType() const override { return ResourceType::Framebuffer; }

protected:

    Framebuffer(Device* device, const std::string& debugName) : IFramebuffer{ device, debugName } {};

    void Release(Device* device) override;
};

} // namespace st::rhi::dx12