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

alm::gfx::SceneGraph::SceneGraph()
{
    m_Root = alm::make_unique_with_weak<alm::gfx::SceneGraphNode>("root");
    m_Root->m_Graph = this;
    m_Root->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
    m_Root->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
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
        }
    }

    node->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
    node->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
}

void alm::gfx::SceneGraph::OnNodeDettached(SceneGraphNode* node)
{
    assert(!node->m_Parent || node->m_Parent->m_Graph == this);

    // Check the parent is valid
    for (auto walker = Walker{ node }; walker; walker.Next())
    {
        const auto& leaf = walker->GetLeaf();
        if (leaf)
        {
            UnregisterLeaf(leaf.get());
        }
        walker->m_Graph = nullptr;
    }

    if (node->m_Parent)
    {
        node->m_Parent->m_DirtyFlags |= SceneGraphNode::DirtyFlags::All;
        node->m_Parent->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
    }
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
    assert(m_Leafs.MeshInstances.valid_index(meshInstanceIndex) && "invalid index");
    return m_Leafs.MeshInstances[meshInstanceIndex]->GetMeshSceneIndex();
}

int alm::gfx::SceneGraph::GetMaterialIndex(const alm::gfx::MeshInstance* pInstance) const
{
    return pInstance->GetMaterialSceneIndex();
}

int alm::gfx::SceneGraph::GetMaterialIndex(const alm::gfx::Mesh* mesh) const
{    
    auto meshRefIdx = m_Meshes.find(const_cast<alm::gfx::Mesh*>(mesh)); // ugly
    if (meshRefIdx != m_Meshes.capacity())
    {
        return m_MeshMaterialIndices[meshRefIdx];
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
        leaf->m_SceneIndex = m_Leafs.MeshInstances.insert(meshInstance);

        const auto& mesh = meshInstance->GetMesh();
        if (mesh)
        {
            // Material
            const auto& mat = mesh->GetMaterial();
            if (mat)
            {
                auto materialIdx = m_Materials.find(mat.get());
                if (materialIdx == m_Materials.capacity())
                {
                    materialIdx = m_Materials.insert({ mat.get(), 1 }).first;
                }
                else
                {
                    ++m_Materials[materialIdx].refCount;
                }
                meshInstance->SetMaterialSceneIndex(materialIdx);
            }

            // Mesh
            auto meshRefIdx = m_Meshes.find(mesh.get());
            if (meshRefIdx == m_Meshes.capacity())
            {
                meshRefIdx = m_Meshes.insert({ mesh.get(), 1 }).first;
                m_MeshMaterialIndices[meshRefIdx] = meshInstance->GetMaterialSceneIndex();
            }
            else
            {
                ++m_Meshes[meshRefIdx].refCount;
            }
            meshInstance->SetMeshSceneIndex(meshRefIdx);
        }
    } break;
        
    case SceneGraphLeaf::Type::Camera:
        leaf->m_SceneIndex = m_Leafs.SceneCameras.insert(checked_cast<SceneCamera*>(leaf));
        break;

    case SceneGraphLeaf::Type::DirectionalLight:
        leaf->m_SceneIndex = m_Leafs.SceneDirLights.insert(checked_cast<SceneDirectionalLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::PointLight:
        leaf->m_SceneIndex = m_Leafs.ScenePointLights.insert(checked_cast<ScenePointLight*>(leaf));
        break;

    case SceneGraphLeaf::Type::SpotLight:
        leaf->m_SceneIndex = m_Leafs.SceneSpotLights.insert(checked_cast<SceneSpotLight*>(leaf));
        break;

    default:
        assert(false && "Unknown leaf type");
    }
}

void alm::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{
    switch (leaf->GetType())
    {
    case SceneGraphLeaf::Type::MeshInstance:
    {
        auto* meshInstance = checked_cast<MeshInstance*>(leaf);
        // Remove material ref
        int materialIdx = meshInstance->GetMaterialSceneIndex();
        if (materialIdx >= 0)
        {
            assert(m_Materials.valid_index(materialIdx));
            assert(m_Materials[materialIdx].refCount > 0);
            --m_Materials[materialIdx].refCount;
            if (m_Materials[materialIdx].refCount == 0)
            {
                m_Materials.erase(materialIdx);
            }
            meshInstance->SetMaterialSceneIndex(-1);
        }

        // Remove mesh ref
        int meshRefIdx = meshInstance->GetMeshSceneIndex();
        if (meshRefIdx >= 0)
        {
            assert(m_Meshes.valid_index(meshRefIdx));
            assert(m_Meshes[meshRefIdx].refCount > 0);
            --m_Meshes[meshRefIdx].refCount;
            if (m_Meshes[meshRefIdx].refCount == 0)
            {
                m_MeshMaterialIndices[meshRefIdx] = -1;
                m_Meshes.erase(meshRefIdx);
            }
            meshInstance->SetMeshSceneIndex(-1);
        }

        // Remove leaf ref
        assert(leaf->m_SceneIndex >= 0);
        assert(m_Leafs.MeshInstances.valid_index(leaf->m_SceneIndex));
        m_Leafs.MeshInstances.erase(leaf->m_SceneIndex);
        leaf->m_SceneIndex = -1;
    } break;

    case SceneGraphLeaf::Type::Camera:
        assert(leaf->m_SceneIndex >= 0);
        assert(m_Leafs.SceneCameras.valid_index(leaf->m_SceneIndex));
        m_Leafs.SceneCameras.erase(leaf->m_SceneIndex);
        leaf->m_SceneIndex = -1;
        break;

    case SceneGraphLeaf::Type::DirectionalLight:
        assert(leaf->m_SceneIndex >= 0);
        assert(m_Leafs.SceneDirLights.valid_index(leaf->m_SceneIndex));
        m_Leafs.SceneDirLights.erase(leaf->m_SceneIndex);
        leaf->m_SceneIndex = -1;
        break;

    case SceneGraphLeaf::Type::PointLight:
        assert(leaf->m_SceneIndex >= 0);
        assert(m_Leafs.ScenePointLights.valid_index(leaf->m_SceneIndex));
        m_Leafs.ScenePointLights.erase(leaf->m_SceneIndex);
        leaf->m_SceneIndex = -1;
        break;

    case SceneGraphLeaf::Type::SpotLight:
        assert(leaf->m_SceneIndex >= 0);
        assert(m_Leafs.SceneSpotLights.valid_index(leaf->m_SceneIndex));
        m_Leafs.SceneSpotLights.erase(leaf->m_SceneIndex);
        leaf->m_SceneIndex = -1;
        break;
    }
}