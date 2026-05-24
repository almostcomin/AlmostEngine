#pragma once

#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"


namespace alm::gfx
{

struct VisibleSetContext;
class GpuSceneBuffers;

struct RenderableDrawInfo
{
    MaterialDomain  MaterialDomain;
    rhi::CullMode   CullMode;

    uintptr_t       BatchKey;

    uint32_t        InstanceIdx;        // Index in the GpuSceneBuffers Instances buffer
    uint32_t        MeshIndex;          // Index in the GpuSceneBuffers Meshes buffer
    uint32_t        MaterialIndex;      // Index in the GpuSceneBuffers Materials buffer
    uint32_t        TransientBaseIndex; // Base index in the GpuSceneBuffer transient instances

    size_t          IndexCount;         // Number of indices to draw
};

class IRenderable
{
public:
    virtual ~IRenderable() = default;
    virtual void CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const = 0;
};

} // namespace alm::gfx