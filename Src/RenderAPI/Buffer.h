#pragma once

#include "RenderAPI/Resource.h"
#include <string>
#include "RenderAPI/Format.h"
#include "RenderAPI/Constants.h"
#include "RenderAPI/Descriptors.h"

namespace st::rapi
{
    // Vertex buffer's are not used in the engine. Rather vertex pulling is used and data is stored in structured buffer.
    enum class BufferUsage
    {
        UploadBuffer,       // implies local mem
        ConstantBuffer,     // implies local mem
        IndexBuffer,        // implies gpu mem
        StructuredBuffer    // implies gpu mem
    };

    struct BufferDesc
    {
        BufferUsage bufferUsage = BufferUsage::StructuredBuffer;
        ResourceUsage resourceUsage = ResourceUsage::ShaderResource;
        size_t sizeBytes = 0;
        bool allowUAV = false;
        uint32_t stride = 0; // only needed for structured buffer types!

        std::string debugName;
    };

	class IBuffer : public IResource
	{
    public:
        virtual const BufferDesc& GetDesc() const = 0;
        virtual GpuVirtualAddress GetGpuVirtualAddress() const = 0;

        virtual void* Map(uint64_t bufferStart = 0, size_t size = 0) = 0;
        virtual void Unmap(uint64_t bufferStart = 0, size_t size = 0) = 0;

        virtual DescriptorIndex GetDescriptorIndex(DescriptorType type) = 0;
	};

	using BufferHandle = std::shared_ptr<IBuffer>;
}