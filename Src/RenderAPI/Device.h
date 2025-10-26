#pragma once

#include "RenderAPI/Texture.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/CommandList.h"

namespace st::rapi
{
	class Device
	{
    public:

        virtual TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) = 0;

        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) = 0;

        virtual CommandListHandle CreateCommandList(const CommandListParams& params) = 0;

        virtual void WaitForIdle() = 0;
	};

} // namespace st::rapi

