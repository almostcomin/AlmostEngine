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
class SceneCamera;
class SceneDirectionalLight;
class ScenePointLight;
class SceneSpotLight;
class SceneHeightmap;
class Mesh;
class Material;
class GpuSceneBuffers;

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

    using MeshInstanceLeafsContainer = std::unordered_set<MeshInstance*>;
    using SceneCamerasContainer = std::unordered_set<SceneCamera*>;
    using SceneDirectionalLightsContainer = std::unordered_set<SceneDirectionalLight*>;
    using ScenePointLightsContainer = std::unordered_set<ScenePointLight*>;
    using SceneSpotLightsContainer = std::unordered_set<SceneSpotLight*>;
    using ScenenHeightmapsContainer = std::unordered_set<SceneHeightmap*>;

public:

    SceneGraph(GpuSceneBuffersHandle buffersHandle, GpuSceneBuffers* gpuSceneBuffers);
    ~SceneGraph();

	// Attaches a node and its subgraph to the graph.
    void OnNodeAttached(SceneGraphNode* node);
    // Detach a node and its subgraph from the graph
    void OnNodeDettached(SceneGraphNode* node);

    // Get root node. Always exists.
    alm::weak<SceneGraphNode> GetRoot() const { return m_Root.get_weak(); }

    const MeshInstanceLeafsContainer& GetMeshInstances() const { return m_Leafs.MeshInstances; }
    const SceneCamerasContainer& GetSceneCameras() const { return m_Leafs.SceneCameras; }
    const SceneDirectionalLightsContainer& GetSceneDirLights() const { return m_Leafs.SceneDirLights; }
    const ScenePointLightsContainer& GetScenePointLights() const { return m_Leafs.ScenePointLights; }
    const SceneSpotLightsContainer& GetSceneSpotLights() const { return m_Leafs.SceneSpotLights; }
    const ScenenHeightmapsContainer& GetSceneHeightmaps() const { return m_Leafs.SceneHeightmaps; }

    // Called when leaf content has changed
    void ReportLeafDirty(const SceneGraphLeaf* leaf);

    // Updates world transforms, bounds and content flags for dirty nodes.
    // Updates the GpuSceneBuffers accordly.
    void Update();

    void LogGraph();

private:

    void UpdateNodeRecursive(SceneGraphNode* node, bool parentTransformChanged);
    void RegisterLeaf(SceneGraphLeaf* leaf);
    void UnregisterLeaf(SceneGraphLeaf* leaf);
    void ReportLeafMoved(const SceneGraphLeaf* leaf);

private:

	alm::unique<SceneGraphNode> m_Root;

    struct
    {
        MeshInstanceLeafsContainer MeshInstances;
        SceneCamerasContainer SceneCameras;
        SceneDirectionalLightsContainer SceneDirLights;
        ScenePointLightsContainer ScenePointLights;
        SceneSpotLightsContainer SceneSpotLights;
        ScenenHeightmapsContainer SceneHeightmaps;
    }  m_Leafs;

    GpuSceneBuffersHandle m_GpuBuffersHandle;
    GpuSceneBuffers* m_GpuSceneBuffers;
};

} // namespace st::gfx