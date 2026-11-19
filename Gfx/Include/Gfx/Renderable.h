#pragma once

#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"


namespace alm::gfx
{

struct VisibleSetContext;
class GpuSceneBuffers;

struct RenderableDrawInfo
{
    MaterialDomain::Type MaterialDomain = MaterialDomain::Opaque;
    rhi::CullMode   CullMode = rhi::CullMode::Back;

    uintptr_t       BatchKey = 0;

    uint32_t        InstanceIdx = 0;        // Index in the GpuSceneBuffers Instances buffer
    uint32_t        MeshIndex = 0;          // Index in the GpuSceneBuffers Meshes buffer
    uint32_t        MaterialIndex = 0;      // Index in the GpuSceneBuffers Materials buffer
    uint32_t        TransientBaseIndex = 0; // Base index in the GpuSceneBuffer transient instances

    size_t          IndexCount = 0;         // Number of indices to draw

    float           ViewDepth = 0.f;        // Squared distance to camera eye (back-to-front sorting)
};

inline int CompareRenderableDrawInfo_Canonical(const RenderableDrawInfo& a, const RenderableDrawInfo& b)
{
    if (a.MaterialDomain != b.MaterialDomain)
        return a.MaterialDomain < b.MaterialDomain;
    if (a.CullMode != b.CullMode)
        return a.CullMode < b.CullMode;
    return a.BatchKey < b.BatchKey;
};

class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual void CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const = 0;
};

} // namespace alm::gfx