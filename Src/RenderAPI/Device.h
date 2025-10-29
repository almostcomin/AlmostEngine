#pragma once

#include "RenderAPI/Buffer.h"
#include "RenderAPI/Texture.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Fence.h"

namespace st::rapi
{
	class Device
	{
    public:

        virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;

        virtual TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) = 0;

        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) = 0;

        virtual CommandListHandle CreateCommandList(const CommandListParams& params) = 0;

        virtual FenceHandle CreateFence() = 0;

        virtual void WaitForIdle() = 0;
	};

} // namespace st::rapi

