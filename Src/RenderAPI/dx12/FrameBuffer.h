#pragma once

#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/dx12/DescriptorHeap.h"

namespace st::rapi::dx12
{

class Framebuffer : public IFramebuffer
{
public:
    FramebufferDesc desc;
    FramebufferInfo framebufferInfo;

    static_vector<TextureHandle, c_MaxRenderTargets + 1> textures;
    static_vector<DescriptorIndex, c_MaxRenderTargets> RTVs;
    DescriptorIndex DSV = c_InvalidDescriptorIndex;
    uint32_t rtWidth = 0;
    uint32_t rtHeight = 0;

    const FramebufferDesc& GetDesc() const override { return desc; }
    const FramebufferInfo& GetFramebufferInfo() const override { return framebufferInfo; }
};

} // namespace st::rapi::dx12