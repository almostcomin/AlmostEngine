#include "Gfx/GfxPCH.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneCamera.h"
#include "Gfx/SceneLights.h"
#include "Gfx/SceneHeigthmap.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Mesh.h"

int alm::gfx::SceneGraph::Walker::Next(IterationMode mode)
{
    if (!m_Current) 
        return 0;

    if (m_Current->GetChildrenCount() > 0)
    {
        m_ChildIndices.push(0);
        m_Current = m_Current->GetChild(0).get();
        return 1;
    }

    return NextSibling(mode);
}

int alm::gfx::SceneGraph::Walker::NextSibling(IterationMode mode)
{
    int depth = 0;
    while (m_Current)
    {
        if (m_Current == m_Scope)
        {
            m_Current = nullptr;
            return depth;
        }

        if (!m_ChildIndices.empty())
        {
            size_t& siblingIndex = m_ChildIndices.top();
            ++siblingIndex;

            auto parent = m_Current->GetParent();
            if (siblingIndex < parent->GetChildrenCount())
            {
                m_Current = parent->GetChild(siblingIndex).get();
                return depth;
            }

            m_ChildIndices.pop();
        }

        m_Current = m_Current->GetParent();
        --depth;
        if (mode == IterationMode::SingleStep)
            return depth;
    }

    return depth;
}

alm::gfx::SceneGraphNode* alm::gfx::SceneGraph::Walker::Up()
{
    if (!m_Current)
        return {};

    if (!m_ChildIndices.empty())
        m_ChildIndices.pop();

    m_Current = m_Current->GetParent();
    return m_Current;
}

alm::gfx::SceneGraph::SceneGraph(GpuSceneBuffersHandle buffersHandle, gfx::GpuSceneBuffers* gpuSceneBuffers) :
    m_GpuBuffersHandle{ buffersHandle }, m_GpuSceneBuffers{ gpuSceneBuffers }
{
    m_Root = alm::make_unique_with_weak<alm::gfx::SceneGraphNode>("root");
    m_Root->m_Graph = this;
    m_Root->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
    m_Root->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
}

alm::gfx::SceneGraph::~SceneGraph()
{
    m_Root->m_Graph = nullptr;
    this->OnNodeDettached(m_Root.get());
}

void alm::gfx::SceneGraph::OnNodeAttached(SceneGraphNode* node)
{
    // If the node has a parent, that parent should be already attached to the graph
    assert(!node->m_Parent || node->m_Parent->m_Graph == this);
    // If the node has no parent, that means it is the (new) root

    // Check the parent is valid
    for (auto walker = Walker{ node }; walker; walker.Next())
    {
        walker->m_Graph = this;
        const auto& leaf = walker->GetLeaf();
        if (leaf)
        {
            RegisterLeaf(leaf.get());
            walker->m_DirtyFlags |= SceneGraphNode::DirtyFlags::Leaf;
        }
    }

    node->m_DirtyFlags |= SceneGraphNode::DirtyFlags::LocalTransform; // Force rebuild world transform & bounds
    node->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
}

void alm::gfx::SceneGraph::OnNodeDettached(SceneGraphNode* node)
{
    assert(!node->m_Parent || node->m_Parent->m_Graph == this);

    bool hasLeaf = false;
    for (auto walker = Walker{ node }; walker; walker.Next())
    {
        const auto& leaf = walker->GetLeaf();
        if (leaf)
        {
            UnregisterLeaf(leaf.get());
            hasLeaf = true;
        }
        walker->m_Graph = nullptr;
    }

    if (node->m_Parent && hasLeaf)
    {
        node->m_Parent->m_DirtyFlags |= SceneGraphNode::DirtyFlags::Leaf;
        node->m_Parent->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
    }
}

void alm::gfx::SceneGraph::Update()
{
    if (m_Root)
    {
        UpdateNodeRecursive(m_Root.get(), false);
    }
}

void alm::gfx::SceneGraph::ReportLeafDirty(const SceneGraphLeaf* leaf)
{
    switch (leaf->GetType())
    {
    case SceneGraphLeaf::Type::MeshInstance:
        m_GpuSceneBuffers->SetDirtyMeshInstance(m_GpuBuffersHandle, checked_cast<const MeshInstance*>(leaf));
        break;
    }
}

void alm::gfx::SceneGraph::LogGraph()
{
    // Call update first to make sure bbox and related data is up to date
    Update();

    alm::gfx::SceneGraph::Walker walker(m_Root.get());
    int depth = 0;
    while (walker)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);

        for (int i = 0; i < depth; i++)
            ss << "   ";

        if (walker->GetName().empty())
            ss << "<Unnamed>";
        else
            ss << walker->GetName();

        if (walker->GetLeaf())
        {
            switch (walker->GetLeaf()->GetType())
            {
            case alm::gfx::SceneGraphLeaf::Type::MeshInstance:
                ss << " : MESH_INSTANCE ";
                break;
            case alm::gfx::SceneGraphLeaf::Type::Camera:
                ss << " : CAMERA ";
                break;
            case alm::gfx::SceneGraphLeaf::Type::PointLight:
                ss << " : POINT_LIGHT ";
                break;
            default:
                ss << " : UNKNOWN_LEAF ";
            }
        }

        if (!ss.str().empty())
            alm::log::Info("{}", ss.str());

        depth += walker.Next();
    }
}

void alm::gfx::SceneGraph::UpdateNodeRecursive(SceneGraphNode* node, bool parentTransformChanged)
{
    if (!has_any_flag(node->m_DirtyFlags, SceneGraphNode::DirtyFlags::All))
        return;

    // Snapshot what changed in THIS node before we clear flags.
    const bool transformChanged = parentTransformChanged ||
        has_any_flag(node->m_DirtyFlags, SceneGraphNode::DirtyFlags::LocalTransform);
    const bool leafChanged =
        has_any_flag(node->m_DirtyFlags, SceneGraphNode::DirtyFlags::Leaf);

    // 1. World transform
    // If our local transform changed, OR our parent's world matrix changed (which means we entered here via Subgraph propagation from above),
    // we need to recompute.
    // The simplest correct rule: recompute if LocalTransform is set on us OR on any ancestor.
    if (transformChanged)
    {
        if (node->m_Parent)
        {
            node->m_WorldMatrix = node->m_Parent->m_WorldMatrix * node->m_LocalTransform.GetMatrix();
        }
        else
        {
            node->m_WorldMatrix = node->m_LocalTransform.GetMatrix();
        }
        node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::LocalTransform;
    }

    // 2. Reset content flags & bounds
    for (int i = 0; i < (int)SceneContentType::_Size; ++i)
    {
        node->m_WorldBounds[i] = math::aabox3f::get_empty();
    }
    node->m_ContentFlags = SceneContentFlags::None;

    // 3. Leaf contribution
    if (node->m_Leaf)
    {
        node->m_ContentFlags = node->m_Leaf->GetContentFlags();
        if (node->m_Leaf->HasBounds())
        {
            const auto& bounds = node->m_Leaf->GetBounds();
            assert(bounds.valid());
            const auto worldBounds = bounds.transform(node->m_WorldMatrix);
            for (int contentTypeIndex = 0; contentTypeIndex < (int)SceneContentType::_Size; ++contentTypeIndex)
            {
                node->m_WorldBounds[contentTypeIndex] = worldBounds;
            }
        }

        if (leafChanged)
        {
            node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::Leaf;
        }

        // If the leaf was just (re)attached, it already went through RegisterMeshInstance
        // and is queued as a new entry — no need to also mark it dirty for the transform.
        if (transformChanged && !leafChanged)
        {
            ReportLeafMoved(node->m_Leaf.get());
        }
    }

    // 4. Recurse and merge
    for (auto& child : node->m_Children)
    {
        UpdateNodeRecursive(child.get(), transformChanged);

        node->m_ContentFlags |= child->m_ContentFlags;
        for (int i = 0; i < (int)SceneContentType::_Size; ++i)
        {
            if (has_any_flag(child->m_ContentFlags, ToFlag((SceneContentType)i)))
                node->m_WorldBounds[i].merge(child->m_WorldBounds[i]);
        }
    }

    // Clear dirty
    node->m_DirtyFlags = SceneGraphNode::DirtyFlags::None;
}

void alm::gfx::SceneGraph::RegisterLeaf(SceneGraphLeaf* leaf)
{
    switch (leaf->GetType())
    {
    case SceneGraphLeaf::Type::MeshInstance:
    {
        MeshInstance* mi = checked_cast<MeshInstance*>(leaf);
        m_GpuSceneBuffers->RegisterMeshInstance(m_GpuBuffersHandle, mi);
        m_Leafs.MeshInstances.insert(mi);
    } break;

    case SceneGraphLeaf::Type::Camera:
        m_Leafs.SceneCameras.insert(checked_cast<SceneCamera*>(leaf));
        break;

    case SceneGraphLeaf::Type::DirectionalLight:
        m_Leafs.SceneDirLights.insert(checked_cast<SceneDirectionalLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::PointLight:
        m_Leafs.ScenePointLights.insert(checked_cast<ScenePointLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::SpotLight:
        m_Leafs.SceneSpotLights.insert(checked_cast<SceneSpotLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::Heightmap:
        m_Leafs.SceneHeightmaps.insert(checked_cast<SceneHeightmap*>(leaf));
        break;
        
    default:
        assert(false && "Type not supported");
    }
}

void alm::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{    
    switch (leaf->GetType())
    {
    case SceneGraphLeaf::Type::MeshInstance:
    {
        MeshInstance* mi = checked_cast<MeshInstance*>(leaf);
        m_GpuSceneBuffers->UnregisterMeshInstance(m_GpuBuffersHandle, mi);
        m_Leafs.MeshInstances.erase(mi);
    } break;

    case SceneGraphLeaf::Type::Camera:
        m_Leafs.SceneCameras.erase(checked_cast<SceneCamera*>(leaf));
        break;

    case SceneGraphLeaf::Type::DirectionalLight:
        m_Leafs.SceneDirLights.erase(checked_cast<SceneDirectionalLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::PointLight:
        m_Leafs.ScenePointLights.erase(checked_cast<ScenePointLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::SpotLight:
        m_Leafs.SceneSpotLights.erase(checked_cast<SceneSpotLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::Heightmap:
        m_Leafs.SceneHeightmaps.erase(checked_cast<SceneHeightmap*>(leaf));
        break;

    default:
        assert(false && "Type not supported");
    }
}

void alm::gfx::SceneGraph::ReportLeafMoved(const SceneGraphLeaf* leaf)
{
    switch (leaf->GetType())
    {
    case SceneGraphLeaf::Type::MeshInstance:
        m_GpuSceneBuffers->SetDirtyMeshInstance(m_GpuBuffersHandle, checked_cast<const MeshInstance*>(leaf));
        break;

    default:
        // Other leaf types (lights, cameras, heightmaps) are not yet
        // managed by GpuSceneBuffers. Nothing to report.
        break;
    }
}