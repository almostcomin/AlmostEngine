#pragma once

#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"


namespace alm::gfx
{

struct RenderableDrawInfo
{
    uint32_t InstanceIdx;
    uint32_t MeshIndex;
    uint32_t MaterialIndex;
    size_t IndexCount;
};

class IRenderable
{
public:
    virtual ~IRenderable() = default;
    virtual MaterialDomain     GetMaterialDomain() const = 0;
    virtual rhi::CullMode      GetCullMode()       const = 0;
    virtual uintptr_t          GetBatchKey()       const = 0;
    virtual RenderableDrawInfo GetDrawInfo()       const = 0;
};

} // namespace alm::gfx