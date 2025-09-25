#pragma once
#include <stack>
#include "Gfx/SceneGraphNode.h"
#include "Core/Memory.h"

namespace st::gfx
{

class SceneGraphNode;

class SceneGraph : public st::enable_weak_from_this<SceneGraph>, private st::noncopyable_nonmovable
{
    friend class SceneGraphNode;

public:

    // Scene graph traversal helper. Similar to an iterator, but only goes forward.
    // Create a Walker from a node, and it will go over every node in the sub-tree of that node.
    // On each location, the walker can move either down (deeper) or right (siblings), depending on the needs.
    class Walker final
    {
    public:

        explicit Walker(SceneGraphNode* scope)
            : m_Current(scope->weak_from_this())
            , m_Scope(scope->weak_from_this())
        {}

        explicit Walker(st::weak<SceneGraphNode> scope)
            : m_Current(scope)
            , m_Scope(scope)
        {}

        Walker(st::weak<SceneGraphNode> current, st::weak<SceneGraphNode> scope)
            : m_Current(current)
            , m_Scope(scope)
        {}

        // Moves the pointer to the first child of the current node, if it exists.
        // Otherwise, moves the pointer to the next sibling of the current node, if it exists.
        // Otherwise, goes up and tries to find the next sibiling up the hierarchy.
        // Returns the depth of the new node relative to the current node.
        st::weak<SceneGraphNode> Next();

        // Moves the pointer to the parent of the current node, up to the scope.
        // Note that using Up and Next together may result in an infinite loop.
        // Returns the depth of the new node relative to the current node.
        st::weak<SceneGraphNode> Up();

        [[nodiscard]] operator bool() const { return !m_Current.expired(); }
        st::weak<SceneGraphNode> Get() const { return m_Current; }
        SceneGraphNode* operator->() { return m_Current.get(); }

    private:
        st::weak<SceneGraphNode> m_Current;
        st::weak<SceneGraphNode> m_Scope;
        std::stack<size_t> m_ChildIndices;
    };

public:

	SceneGraph() = default;

	// Replaces the current root node of the graph with the new one. Returns the old root.
	st::unique<st::gfx::SceneGraphNode> SetRoot(st::unique<SceneGraphNode>&& rootNode);

	// Attaches a node and its subgraph to the parent.
	// If the node is already attached to this or other graph, a deep copy of the subgraph is made first.
	st::weak<SceneGraphNode> Attach(SceneGraphNode* parent, st::unique<SceneGraphNode>&& child);

    // Removes the node and its subgraph from the graph.
    st::unique<SceneGraphNode> Detach(const SceneGraphNode* node);

private:

    void RegisterLeaf(SceneGraphLeaf* leaf);
    void UnregisterLeaf(SceneGraphLeaf* leaf);

private:

	st::unique<SceneGraphNode> m_Root;
};

} // namespace st::gfx