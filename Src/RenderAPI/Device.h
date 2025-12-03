#pragma once

#include "Core/Blob.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Texture.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Fence.h"
#include "RenderAPI/Barriers.h"
#include "RenderAPI/Shader.h"
#include "RenderAPI/PipelineState.h"

namespace st::rapi
{
	class Device
	{
    public:

        virtual ShaderHandle CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode) = 0;
        virtual BufferHandle CreateBuffer(const BufferDesc& desc, ResourceState initialState) = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& desc, ResourceState initialState) = 0;
        virtual TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) = 0;
        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) = 0;
        virtual CommandListHandle CreateCommandList(const CommandListParams& params) = 0;
        virtual GraphicsPipelineStateHandle CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo) = 0;
        virtual FenceHandle CreateFence(uint64_t initialVale = 0, const char* debugName = nullptr) = 0;

        virtual void ReleaseImmediately(const weak<IResource>& handle) = 0;
        virtual void ReleaseQueued(const weak<IResource>& handle) = 0;

        virtual void ExecuteCommandLists(std::span<ICommandList*> commandLists, QueueType type, IFence* signal = nullptr, uint64_t value = 0) = 0;
        virtual void ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal = nullptr, uint64_t value = 0) = 0;

        virtual void WaitForIdle() = 0;

        virtual void NextFrame() = 0;

        virtual void Shutdown() = 0;

    protected:

        static void ResourceDeleter(IResource* p) {
            delete p;
        }

        void ReleaseResource(IResource* p) {
            p->Release(this);
        }
	};

} // namespace st::rapi

