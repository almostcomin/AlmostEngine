#pragma once
#include <stack>
#include "Gfx/SceneGraphNode.h"
#include "Gfx/ResourceRefCount.h"
#include "Core/Memory.h"
#include "Core/unique_vector.h"
#include "Core/unique_stable_vector.h"
#include "Core/stable_vector.h"

namespace alm::gfx
{

class SceneGraphNode;
class MeshInstance;
class SceneCamera;
class SceneDirectionalLight;
class ScenePointLight;
class SceneSpotLight;
class Mesh;
class Material;

class SceneGraph : public alm::enable_weak_from_this<SceneGraph>, private alm::noncopyable_nonmovable
{
    friend class SceneGraphNode;
    
public:

    // Scene graph traversal helper. Similar to an iterator, but only goes forward.
    // Create a Walker from a node, and it will go over every node in the sub-tree of that node.
    // On each location, the walker can move either down (deeper) or right (siblings), depending on the needs.
    class Walker final
    {
    public:

        enum class IterationMode
        {
            MultiStep,
            SingleStep
        };

        explicit Walker(const SceneGraph& graph)
            : m_Current(graph.m_Root.get())
            , m_Scope(graph.m_Root.get())
        {}

        explicit Walker(SceneGraphNode* scope)
            : m_Current(scope)
            , m_Scope(scope)
        {}
/*
        explicit Walker(alm::weak<SceneGraphNode> scope)
            : m_Current(scope)
            , m_Scope(scope)
        {}
*/
/*
        Walker(alm::weak<SceneGraphNode> current, alm::weak<SceneGraphNode> scope)
            : m_Current(current)
            , m_Scope(scope)
        {}
*/
        // Moves the pointer to the first child of the current node, if it exists.
        // Otherwise, moves the pointer to the next sibling of the current node, if it exists.
        // Otherwise, goes up and tries to find the next sibiling up the hierarchy.
        // Returns the depth of the new node relative to the current node.
        int Next(IterationMode mode = IterationMode::MultiStep);

        int NextSibling(IterationMode mode = IterationMode::MultiStep);

        // Moves the pointer to the parent of the current node, up to the scope.
        // Note that using Up and Next together may result in an infinite loop.
        // Returns the depth of the new node relative to the current node.
        SceneGraphNode* Up();

        [[nodiscard]] operator bool() const { return m_Current; }
        SceneGraphNode* Get() const { return m_Current; }
        SceneGraphNode* operator*() { return m_Current; }
        SceneGraphNode* operator->() { return m_Current; }

    private:
        SceneGraphNode* m_Current;
        SceneGraphNode* m_Scope;
        std::stack<size_t> m_ChildIndices;
    };

    using MaterialRefCount = ResourceRefCount<Material>;
    using MeshRefCount = ResourceRefCount<Mesh>;
   
    using MeshInstanceLeafsContainer = alm::stable_vector<MeshInstance*, 8192>;
    using SceneCamerasContainer = alm::stable_vector<SceneCamera*, 32>;
    using SceneDirectionalLightsContainer = alm::stable_vector<SceneDirectionalLight*, 8>;
    using ScenePointLightsContainer = alm::stable_vector<ScenePointLight*, 1024>;
    using SceneSpotLightsContainer = alm::stable_vector<SceneSpotLight*, 128>;

    using MaterialsContainer = alm::unique_stable_vector<MaterialRefCount, 1024>;
    using MeshesContainer = alm::unique_stable_vector<MeshRefCount, 4096>;
    using MeshMaterialIndicesContainer = std::array<int, MeshesContainer::max_elements>;

public:

    SceneGraph();

	// Attaches a node and its subgraph to the graph.
    void OnNodeAttached(SceneGraphNode* node);
    // Detach a node and its subgraph from the graph
    void OnNodeDettached(SceneGraphNode* node);

    // Get root node. Always exists.
    alm::weak<SceneGraphNode> GetRoot() const { return m_Root.get_weak(); }

    int GetInstanceIndex(const alm::gfx::MeshInstance* pInstance) const;
    
    int GetMeshIndex(const alm::gfx::MeshInstance* pInstance) const;
    int GetMeshIndex(int meshInstanceIndex) const;
    
    int GetMaterialIndex(const alm::gfx::MeshInstance* pInstance) const;
    int GetMaterialIndex(const alm::gfx::Mesh* mesh) const;

    const MeshInstanceLeafsContainer& GetMeshInstances() const { return m_Leafs.MeshInstances; }
    const SceneCamerasContainer& GetSceneCameras() const { return m_Leafs.SceneCameras; }
    const SceneDirectionalLightsContainer& GetSceneDirLights() const { return m_Leafs.SceneDirLights; }
    const ScenePointLightsContainer& GetScenePointLights() const { return m_Leafs.ScenePointLights; }
    const SceneSpotLightsContainer& GetSceneSpotLights() const { return m_Leafs.SceneSpotLights; }

    const MeshesContainer& GetMeshes() const { return m_Meshes; }
    const MeshMaterialIndicesContainer& GetMeshesMaterialIndices() const { return m_MeshMaterialIndices; }
    const MaterialsContainer& GetMaterials() const { return m_Materials; }

    // 
    void Refresh();

    void LogGraph() const;

private:

    void UpdateNodeRecursive(SceneGraphNode* node);
    void RegisterLeaf(SceneGraphLeaf* leaf);
    void UnregisterLeaf(SceneGraphLeaf* leaf);

private:

	alm::unique<SceneGraphNode> m_Root;

    struct
    {
        MeshInstanceLeafsContainer MeshInstances;
        SceneCamerasContainer SceneCameras;
        SceneDirectionalLightsContainer SceneDirLights;
        ScenePointLightsContainer ScenePointLights;
        SceneSpotLightsContainer SceneSpotLights;
    }  m_Leafs;

    MeshesContainer m_Meshes;
    MaterialsContainer m_Materials;
    MeshMaterialIndicesContainer m_MeshMaterialIndices;
};

} // namespace st::gfx