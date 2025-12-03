#pragma once

#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/dx12/DescriptorHeap.h"

namespace st::rapi::dx12
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

    ResourceType GetResourceType() const override { return ResourceType::Framebuffer; }

    std::string& GetDebugName() override { return desc.DebugName; }

protected:

    void Release(Device* device) override;
};

} // namespace st::rapi::dx12