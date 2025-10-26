#pragma once

#include "RenderAPI/Resource.h"
#include <string>
#include "RenderAPI/Format.h"
#include "RenderAPI/Constants.h"

namespace st::rapi
{
    // Vertex buffer's are not used in the engine. Rather vertex pulling is used and data is stored in structured buffer.
    enum class BufferUsage
    {
        UploadBuffer,
        IndexBuffer,
        StructuredBuffer,
        ConstantBuffer,
    };

    struct BufferDesc
    {
        BufferUsage usage;
        std::string debugName;
    };

	class IBuffer : public IResource
	{
    public:
        [[nodiscard]] virtual const BufferDesc& GetDesc() const = 0;
        [[nodiscard]] virtual GpuVirtualAddress GetGpuVirtualAddress() const = 0;
	};

	using BufferHandle = std::shared_ptr<IBuffer>;
}