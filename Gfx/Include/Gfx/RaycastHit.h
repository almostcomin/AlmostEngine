#pragma once
#include <stack>
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/ResourceRefCount.h"
#include "Gfx/GpuSceneBuffersHandle.h"
#include "Core/Memory.h"
#include "Core/unique_vector.h"
#include "Core/unique_stable_vector.h"
#include "Core/stable_vector.h"

namespace alm::gfx
{

class SceneGraphNode;
class MeshInstance;

struct RaycastHit
{
    float3 Position;
    float3 Normal;
    float  Distance;
    SceneGraphNode* Node;
    MeshInstance* Instance;
    uint32_t PrimitiveIndex;
};

} // namespace st::gfx