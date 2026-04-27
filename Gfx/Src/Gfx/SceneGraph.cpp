#include "Gfx/GfxPCH.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneCamera.h"
#include "Gfx/SceneLights.h"
#include "Gfx/Mesh.h"

int alm::gfx::SceneGraph::Walker::Next(IterationMode mode)
{
    if (!m_Current) 
        return 0;

    if (m_Current->GetChildrenCount() > 0)
    {
        m_ChildIndices.push(0);
        m_Current = m_Current->GetChild(0);
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
            m_Current.reset();
            return depth;
        }

        if (!m_ChildIndices.empty())
        {
            size_t& siblingIndex = m_ChildIndices.top();
            ++siblingIndex;

            auto parent = m_Current->GetParent();
            if (siblingIndex < parent->GetChildrenCount())
            {
                m_Current = parent->GetChild(siblingIndex);
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

alm::weak<alm::gfx::SceneGraphNode> alm::gfx::SceneGraph::Walker::Up()
{
    if (!m_Current)
        return {};
/*
    if (m_Current == m_Scope)
    {
        m_Current.reset();
        return {};
    }
*/
    if (!m_ChildIndices.empty())
        m_ChildIndices.pop();

    m_Current = m_Current->GetParent();
    return m_Current;
}

alm::gfx::SceneGraph::SceneGraph()
{
}

alm::gfx::SceneGraph::SceneGraph(alm::unique<alm::gfx::SceneGraphNode>&& rootNode)
{
    SetRoot(std::move(rootNode));
}

alm::unique<alm::gfx::SceneGraphNode> alm::gfx::SceneGraph::SetRoot(alm::unique<alm::gfx::SceneGraphNode>&& rootNode)
{
    alm::unique<alm::gfx::SceneGraphNode> oldRoot;
    if (m_Root)
    {
        oldRoot = Detach(m_Root.get());
    }

    m_Root = std::move(rootNode);
    OnNodeAttached(m_Root.get());

    return oldRoot;
}

void alm::gfx::SceneGraph::OnNodeAttached(SceneGraphNode* node)
{
    // If the node has a parent, that parent should be already attached to the graph
    assert(!node->m_Parent || node->m_Parent->m_Graph.get() == this);
    // If the node has no parent, that means it is the (new) root

    // Check the parent is valid
    for (auto walker = Walker{ node }; walker; walker.Next())
    {
        walker->m_Graph = weak_from_this();
        const auto& leaf = walker->GetLeaf();
        if (leaf)
        {
            RegisterLeaf(leaf.get());
        }
    }

    node->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
    node->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
}

#if 0
alm::weak<alm::gfx::SceneGraphNode> alm::gfx::SceneGraph::Attach(SceneGraphNode* parent, alm::unique<SceneGraphNode>&& child)
{
    auto parentGraph = parent ? parent->m_Graph : weak_from_this();
    auto childGraph = child->m_Graph;
    alm::weak<SceneGraphNode> attachedChild = child.get_weak();

    if (!parentGraph && !childGraph)
    {
        // operating on an orphaned subgraph - do not copy or register anything
        assert(parent);
        parent->m_Children.push_back(std::move(child));
        attachedChild->m_Parent = parent->weak_from_this();
        return attachedChild;
    }

    assert(parentGraph.get() == this);

    if (childGraph)
    {
        // TODO
        assert(0);
    }
    else
    {
        // attaching a subgraph that has been detached from another graph (or never attached)

        for (auto walker = Walker{ child.get_weak() }; walker; walker.Next())
        {
            walker->m_Graph = weak_from_this();
            const auto& leaf = walker->GetLeaf();
            if (leaf)
            {
                RegisterLeaf(leaf.get());
            }
        }

        if (parent)
        {
            child->m_Parent = parent->weak_from_this();
            parent->m_Children.push_back(std::move(child));
        }
        else
        {
            m_Root = std::move(child);
        }
    }

    // Force update subgraph next
    attachedChild->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
    attachedChild->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);

    return attachedChild;
}
#endif

alm::unique<alm::gfx::SceneGraphNode> alm::gfx::SceneGraph::Detach(
    const SceneGraphNode* node)
{
    assert(0);
    // TODO
    return {};
}

int alm::gfx::SceneGraph::GetInstanceIndex(const alm::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetLeafSceneIndex();
}

int alm::gfx::SceneGraph::GetMeshIndex(const alm::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetMeshSceneIndex();
}

int alm::gfx::SceneGraph::GetMeshIndex(int meshInstanceIndex) const
{
    assert(meshInstanceIndex < m_MeshInstanceLeafs.size() && "index out of bounds");
    assert(m_MeshInstanceLeafs[meshInstanceIndex] != nullptr && "invalid index");

    return m_MeshInstanceLeafs[meshInstanceIndex]->GetMeshSceneIndex();
}

int alm::gfx::SceneGraph::GetMaterialIndex(const alm::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetMaterialSceneIndex();
}

int alm::gfx::SceneGraph::GetMaterialIndex(const alm::gfx::Mesh* mesh) const
{
    auto it = std::find(m_Meshes.begin(), m_Meshes.end(), mesh);
    if (it != m_Meshes.end())
    {
        int idx = it - m_Meshes.begin();
        assert(idx < m_MeshMaterialIndices.size());
        return m_MeshMaterialIndices[idx];
    }

    return -1;
}

void alm::gfx::SceneGraph::Refresh()
{
    if (!m_Root)
        return;

    UpdateNodeRecursive(m_Root.get());
}

void alm::gfx::SceneGraph::LogGraph() const
{
    alm::gfx::SceneGraph::Walker walker(m_Root.get_weak());
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

void alm::gfx::SceneGraph::UpdateNodeRecursive(SceneGraphNode* node)
{
    if (!has_any_flag(node->m_DirtyFlags, SceneGraphNode::DirtyFlags::All))
        return;

    // 1. World transform
    if (node->m_Parent)
    {
        node->m_WorldMatrix = node->m_Parent->m_WorldMatrix * node->m_LocalTransform.GetMatrix();
    }
    else
    {
        node->m_WorldMatrix = node->m_LocalTransform.GetMatrix();
    }
    node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::LocalTransform;

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
    }

    // 4. Recurse and merge
    for (auto& child : node->m_Children)
    {
        UpdateNodeRecursive(child.get());

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
        auto* meshInstance = checked_cast<MeshInstance*>(leaf);
        m_MeshInstanceLeafs.push_back(meshInstance);
        leaf->m_SceneIndex = m_MeshInstanceLeafs.size() - 1;

        const auto& mesh = meshInstance->GetMesh();
        if (mesh)
        {
            // Material
            const auto& mat = mesh->GetMaterial();
            if (mat)
            {
                int materialIdx = m_Materials.insert(mat.get()).first;
                meshInstance->SetMaterialSceneIndex(materialIdx);
            }

            // Mesh
            auto [meshIdx, newMesh] = m_Meshes.insert(mesh.get());
            meshInstance->SetMeshSceneIndex(meshIdx);
            if (newMesh)
            {
                m_MeshMaterialIndices.push_back(meshInstance->GetMaterialSceneIndex());
            }
            assert(m_MeshMaterialIndices.size() == m_Meshes.size());
        }
    } break;
        
    case SceneGraphLeaf::Type::Camera:
        m_SceneCameraLeafs.push_back(checked_cast<SceneCamera*>(leaf));
        leaf->m_SceneIndex = m_SceneCameraLeafs.size() - 1;
        break;

    case SceneGraphLeaf::Type::DirectionalLight:
        m_SceneDirLightLeafs.push_back(checked_cast<SceneDirectionalLight*>(leaf));
        leaf->m_SceneIndex = m_SceneDirLightLeafs.size() - 1;
        break;

    case SceneGraphLeaf::Type::PointLight:
        m_ScenePointLightLeafs.push_back(checked_cast<ScenePointLight*>(leaf));
        leaf->m_SceneIndex = m_ScenePointLightLeafs.size() - 1;
        break;

    case SceneGraphLeaf::Type::SpotLight:
        m_SceneSpotLightLeafs.push_back(checked_cast<SceneSpotLight*>(leaf));
        leaf->m_SceneIndex = m_SceneSpotLightLeafs.size() - 1;
        break;

    default:
        assert(false && "Unknown leaf type");
    }
}

void alm::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{
    // TODO
}