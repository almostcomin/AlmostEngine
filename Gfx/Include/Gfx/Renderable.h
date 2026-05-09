#pragma once

#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"


namespace alm::gfx
{

struct VisibleSetContext;

struct RenderableDrawInfo
{
    MaterialDomain  MaterialDomain;
    rhi::CullMode   CullMode;
    uintptr_t       BatchKey;
    uint32_t        InstanceIdx;
    uint32_t        MeshIndex;
    uint32_t        MaterialIndex;
    size_t          IndexCount;
};

class IRenderable
{
public:
    virtual ~IRenderable() = default;
    virtual void CollectDrawInfos(const VisibleSetContext& context, std::vector<RenderableDrawInfo>& out) const = 0;
};

} // namespace alm::gfx