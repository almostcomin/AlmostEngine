#pragma once

#include "RHI/Resource.h"
#include "RHI/Format.h"
#include "RHI/Descriptors.h"
#include "RHI/Common.h"

namespace st::rhi
{

struct BufferDesc
{
    MemoryAccess memoryAccess = MemoryAccess::Default;
    BufferShaderUsage shaderUsage = BufferShaderUsage::None;
    size_t sizeBytes = 0;
    bool allowUAV = false;
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

    virtual DescriptorIndex GetShaderViewIndex(BufferShaderView type) = 0;
protected:

    IBuffer(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace st::rhi