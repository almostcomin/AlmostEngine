#pragma once

#include "RenderAPI/Texture.h"
#include "RenderAPI/Framebuffer.h"

namespace st::rapi
{
    enum class CommandQueue : uint8_t
    {
        Graphics = 0,
        Compute,
        Copy,

        Count
    };

	class Device
	{
    public:

        virtual TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) = 0;

        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) = 0;

        virtual void WaitForIdle() = 0;
	};

} // namespace st::rapi

