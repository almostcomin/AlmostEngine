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
    using GPUBindingHandle = uint64_t;

    struct Stats
    {
        uint32_t DrawCalls;
        uint32_t PrimitiveCount;
        float ElapsedGpuMs;
    };

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
        virtual ComputePipelineStateOwner CreateComputePipelineState(const ComputePipelineStateDesc& desc, const std::string& debugName) = 0;
        virtual FenceOwner CreateFence(uint64_t initialVale, const std::string& debugName) = 0;
        virtual TimerQueryOwner CreateTimerQuery(const std::string& debugName) = 0;

        virtual StorageRequirements GetStorageRequirements(const BufferDesc& desc) = 0;
        virtual StorageRequirements GetStorageRequirements(const TextureDesc& desc) = 0;
        virtual StorageRequirements GetCopyableRequirements(const BufferDesc& desc) = 0;
        virtual StorageRequirements GetCopyableRequirements(const TextureDesc& desc, const rhi::TextureSubresourceSet& subresources) = 0;
        virtual SubresourceCopyableRequirements GetSubresourceCopyableRequirements(const TextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice) = 0;

        virtual size_t GetCopyDataAlignment(CopyMethod method) = 0;

        virtual TextureSampledView CreateTextureSampledView(ITexture* texture, const TextureSubresourceSet& subresources = AllSubresources,
            Format format = Format::UNKNOWN, TextureDimension dimension = TextureDimension::Unknown) = 0;
        virtual TextureStorageView CreateTextureStorageView(ITexture* texture, const TextureSubresourceSet& subresources = AllSubresources,
            Format format = Format::UNKNOWN, TextureDimension dimension = TextureDimension::Unknown) = 0;
        virtual TextureColorTargetView CreateTextureColorTargetView(ITexture* texture, Format format, TextureSubresourceSet subresources = AllSubresources) = 0;
        virtual TextureDepthTargetView CreateTextureDepthTargetView(ITexture* texture, TextureSubresourceSet subresources = AllSubresources, bool isReadOnly = false) = 0;

        virtual void ReleaseTextureSampledView(TextureSampledView& v, bool immediate = false) = 0;
        virtual void ReleaseTextureStorageView(TextureStorageView& v, bool immediate = false) = 0;
        virtual void ReleaseTextureColorTargetView(TextureColorTargetView& v, bool immediate = false) = 0;
        virtual void ReleaseTextureDepthTargetView(TextureDepthTargetView& v, bool immediate = false) = 0;

        virtual BufferUniformView CreateBufferUniformView(IBuffer* buffer, uint32_t start = 0, int size = -1) = 0;
        virtual BufferReadOnlyView CreateBufferReadOnlyView(IBuffer* buffer, uint32_t start = 0, int size = -1) = 0;
        virtual BufferReadWriteView CreateBufferReadWriteView(IBuffer* buffer, uint32_t start = 0, int size = -1) = 0;

        virtual void ReleaseBufferUniformView(BufferUniformView& v, bool immediate = false) = 0;
        virtual void ReleaseBufferReadOnlyView(BufferReadOnlyView& v, bool immediate = false) = 0;
        virtual void ReleaseBufferReadWriteView(BufferReadWriteView& v, bool immediate = false) = 0;

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

        virtual const Stats& GetStats() const = 0;

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

