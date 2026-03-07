#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneCamera.h"
#include "Gfx/SceneLights.h"
#include "Gfx/Mesh.h"

int st::gfx::SceneGraph::Walker::Next(IterationMode mode)
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

int st::gfx::SceneGraph::Walker::NextSibling(IterationMode mode)
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

st::weak<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Walker::Up()
{
    if (!m_Current)
        return {};

    if (m_Current == m_Scope)
    {
        m_Current.reset();
        return {};
    }

    if (!m_ChildIndices.empty())
        m_ChildIndices.pop();

    m_Current = m_Current->GetParent();
    return m_Current;
}

st::gfx::SceneGraph::SceneGraph(st::unique<st::gfx::SceneGraphNode>&& rootNode)
{
    SetRoot(std::move(rootNode));
}

st::unique<st::gfx::SceneGraphNode> st::gfx::SceneGraph::SetRoot(st::unique<st::gfx::SceneGraphNode>&& rootNode)
{
    st::unique<st::gfx::SceneGraphNode> oldRoot;
    if (m_Root)
    {
        oldRoot = Detach(m_Root.get());
    }

    Attach(nullptr, std::move(rootNode));

    return oldRoot;
}

st::weak<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Attach(SceneGraphNode* parent, st::unique<SceneGraphNode>&& child)
{
    auto parentGraph = parent ? parent->m_Graph : weak_from_this();
    auto childGraph = child->m_Graph;
    st::weak<SceneGraphNode> attachedChild = child.get_weak();

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

st::unique<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Detach(
    const SceneGraphNode* node)
{
    assert(0);
    // TODO
    return {};
}

int st::gfx::SceneGraph::GetInstanceIndex(const st::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetLeafSceneIndex();
}

int st::gfx::SceneGraph::GetMeshIndex(const st::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetMeshSceneIndex();
}

int st::gfx::SceneGraph::GetMeshIndex(int meshInstanceIndex) const
{
    assert(meshInstanceIndex < m_MeshInstanceLeafs.size() && "index out of bounds");
    assert(m_MeshInstanceLeafs[meshInstanceIndex] != nullptr && "invalid index");

    return m_MeshInstanceLeafs[meshInstanceIndex]->GetMeshSceneIndex();
}

int st::gfx::SceneGraph::GetMaterialIndex(const st::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetMaterialSceneIndex();
}

int st::gfx::SceneGraph::GetMaterialIndex(const st::gfx::Mesh* mesh) const
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

void st::gfx::SceneGraph::Refresh()
{
    // Root dirty?
    if (has_any_flag(m_Root->m_DirtyFlags, SceneGraphNode::DirtyFlags::Subgraph))
    {
        Walker walker{ m_Root.get_weak() };
        while (walker)
        {
            if (has_any_flag(walker->m_DirtyFlags, (SceneGraphNode::DirtyFlags::LocalTransform | SceneGraphNode::DirtyFlags::Leaf)))
            {
                // --- Subgraph needs update

                Walker subgraphWalker{ *walker };
                while (subgraphWalker)
                {
                    auto node = *subgraphWalker;

                    // Update world transform
                    if (node->m_Parent)
                    {
                        node->m_WorldMatrix = node->m_Parent->m_WorldMatrix * node->m_LocalTransform.GetMatrix();
                    }
                    else
                    {
                        node->m_WorldMatrix = node->m_LocalTransform.GetMatrix();
                    }
                    node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::LocalTransform;

                    // Reset world bounds using leaf data
                    node->m_HasBounds = { false };
                    if (node->m_Leaf && node->m_Leaf->HasBounds())
                    {
                        BoundsType boundsType = node->m_Leaf->GetBoundsType();
                        assert(boundsType != BoundsType::_Size);
                        node->m_WorldBounds[(int)boundsType] = node->m_Leaf->GetBounds().transform(node->m_WorldMatrix);
                        node->m_HasBounds[(int)boundsType] = true;
                    }

                    // Set content flags
                    if (node->m_Leaf)
                    {
                        node->m_ContentFlags = node->m_Leaf->GetContentFlags();
                    }
                    else
                    {
                        node->m_ContentFlags = SceneContentFlags::None;
                    }

                    node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::Leaf;

                    int depth = subgraphWalker.Next();

                    // Update parent bounds and content flags if next is sibling or parent
                    const auto& hasBounds = node->m_HasBounds;
                    bool updateBounds = st::any(hasBounds);
                    bool updateContentFlags = node->m_ContentFlags != 0;
                    if ((updateBounds || updateContentFlags) && depth <= 0) // Sibling or going up.
                    {
                        for (int i = depth; i <= 0; ++i)
                        {
                            if (!node->m_Parent)
                                break;

                            // Update parent bounds
                            if (updateBounds)
                            {
                                for (int boundsIdx = 0; boundsIdx < (int)BoundsType::_Size; ++boundsIdx)
                                {
                                    if (hasBounds[boundsIdx])
                                    {
                                        node->m_Parent->m_WorldBounds[boundsIdx].merge(node->m_WorldBounds[boundsIdx]);
                                        node->m_Parent->m_HasBounds[boundsIdx] = true;
                                    }
                                }
                            }

                            // Update content flags
                            if (updateContentFlags)
                            {
                                node->m_Parent->m_ContentFlags |= node->m_ContentFlags;
                            }

                            node = node->m_Parent;
                        }
                    }
                } // Subgraph loop

                // Since we have updated a sub-graph, we need to update parent bounds
                if (st::any(walker->m_HasBounds))
                {
                    auto node = *walker;
                    while (node->m_Parent)
                    {
                        for (int boundsIdx = 0; boundsIdx < (int)BoundsType::_Size; ++boundsIdx)
                        {
                            if (node->m_HasBounds[boundsIdx])
                            {
                                node->m_Parent->m_WorldBounds[boundsIdx].merge(node->m_WorldBounds[boundsIdx]);
                                node->m_Parent->m_HasBounds[boundsIdx] = true;
                            }
                        }
                        node = node->m_Parent;
                    }
                }

                // Subgraph updated, need to go to the next sibling
                walker->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::Subgraph;
                walker.NextSibling();
            }

            else
            {
                // --- Subgraph does NOT need update
                walker.NextSibling();
            }
        }
    } // Root is dirty
}

void st::gfx::SceneGraph::RegisterLeaf(SceneGraphLeaf* leaf)
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

    case SceneGraphLeaf::Type::PointLight:
        m_ScenePointLightLeafs.push_back(checked_cast<ScenePointLight*>(leaf));
        leaf->m_SceneIndex = m_ScenePointLightLeafs.size() - 1;
        break;

    default:
        assert(false && "Unknown leaf type");
    }
    // TODO
}

void st::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{
    // TODO
}