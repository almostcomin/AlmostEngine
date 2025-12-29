#pragma once

#include "Core/Blob.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Fence.h"
#include "RHI/Barriers.h"
#include "RHI/Shader.h"
#include "RHI/PipelineState.h"
#include "RHI/Common.h"

namespace st::rhi
{
    class IResource;

	class Device
	{
        friend class IResource;

    public:

        virtual ShaderOwner CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode, const std::string& debugName) = 0;
        virtual BufferOwner CreateBuffer(const BufferDesc& desc, ResourceState initialState, const std::string& debugName) = 0;
        virtual TextureOwner CreateTexture(const TextureDesc& desc, ResourceState initialState, const std::string& debugName) = 0;
        virtual TextureOwner CreateHandleForNativeTexture(void* obj, const TextureDesc& desc, const std::string& debugName) = 0;
        virtual FramebufferOwner CreateFramebuffer(const FramebufferDesc& desc, const std::string& debugName) = 0;
        virtual CommandListOwner CreateCommandList(const CommandListParams& params, const std::string& debugName) = 0;
        virtual GraphicsPipelineStateOwner CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo, const std::string& debugName) = 0;
        virtual FenceOwner CreateFence(uint64_t initialVale = 0, const std::string& debugName = nullptr) = 0;

        virtual StorageRequirements GetStorageRequirements(const BufferDesc& desc) = 0;
        virtual StorageRequirements GetStorageRequirements(const TextureDesc& desc) = 0;
        virtual StorageRequirements GetCopyableRequirements(const BufferDesc& desc) = 0;
        virtual StorageRequirements GetCopyableRequirements(const TextureDesc& desc) = 0;
        virtual SubresourceCopyableRequirements GetSubresourceCopyableRequirements(const TextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice) = 0;

        virtual size_t GetCopyDataAlignment(CopyMethod method) = 0;


        template<class T>
        void ReleaseImmediately(unique<T>& handle) {
            if (handle)
                ReleaseImmediatelyInternal(handle.release());
        }
        template<class T>
        void ReleaseQueued(unique<T>& handle) {
            if (handle)
                ReleaseQueuedInternal(handle.release());
        }

        virtual void ExecuteCommandLists(std::span<ICommandList*> commandLists, QueueType type, IFence* signal = nullptr, uint64_t value = 0) = 0;
        virtual void ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal = nullptr, uint64_t value = 0) = 0;

        virtual void WaitForIdle() = 0;

        virtual void NextFrame() = 0;

        virtual void Shutdown() = 0;

    protected:

        virtual void ReleaseImmediatelyInternal(IResource* resource) = 0;
        virtual void ReleaseQueuedInternal(IResource* resource) = 0;

        inline static st::unique<IResource> MakeIResourceUnique(IResource* r) {
            return st::unique<IResource>{ r, [](void* p) { static_cast<IResource*>(p)->m_Device->ReleaseQueuedInternal(static_cast<IResource*>(p)); }};
        }

        inline static void ResourceDeleter(IResource* p) {
            delete p;
        }

        inline void ReleaseResource(IResource* p) {
            p->Release(this);
        }
	};

} // namespace st::rhi

