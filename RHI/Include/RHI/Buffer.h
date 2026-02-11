#pragma once

#include "RHI/Resource.h"
#include "RHI/Format.h"
#include "RHI/Descriptors.h"
#include "RHI/Common.h"
#include "ShaderViews.h"

namespace st::rhi
{

struct BufferDesc
{
    MemoryAccess memoryAccess = MemoryAccess::Default;
    BufferShaderUsage shaderUsage = BufferShaderUsage::None;
    size_t sizeBytes = 0;
    Format format = Format::UNKNOWN; // For typed views
    uint32_t stride = 0; // set stride for structured buffer
};

class IBuffer : public IResource
{
public:

    virtual const BufferDesc& GetDesc() const = 0;
    virtual GpuVirtualAddress GetGpuVirtualAddress() const = 0;

    virtual void* Map(uint64_t bufferStart = 0, size_t size = 0) = 0;
    virtual void Unmap(uint64_t bufferStart = 0, size_t size = 0) = 0;

    virtual BufferUniformView GetUniformView() = 0;
    virtual BufferReadOnlyView GetReadOnlyView() = 0;
    virtual BufferReadWriteView GetReadWriteView() = 0;

    virtual void Swap(IBuffer& other) = 0;

protected:

    IBuffer(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace st::rhi