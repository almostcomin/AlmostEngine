#pragma once
#include <memory>
#include <stack>
#include "Gfx/SceneGraphNode.h"

namespace st::gfx
{

class SceneGraphNode;

class SceneGraph : public std::enable_shared_from_this<SceneGraph>, private st::noncopyable_nonmovable
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
            : m_Current(scope)
            , m_Scope(scope)
        {}

        Walker(SceneGraphNode* current, SceneGraphNode* scope)
            : m_Current(current)
            , m_Scope(scope)
        {}

        // Moves the pointer to the first child of the current node, if it exists.
        // Otherwise, moves the pointer to the next sibling of the current node, if it exists.
        // Otherwise, goes up and tries to find the next sibiling up the hierarchy.
        // Returns the depth of the new node relative to the current node.
        SceneGraphNode* Next();

        // Moves the pointer to the parent of the current node, up to the scope.
        // Note that using Up and Next together may result in an infinite loop.
        // Returns the depth of the new node relative to the current node.
        SceneGraphNode* Up();

        [[nodiscard]] operator bool() const { return m_Current != nullptr; }
        SceneGraphNode* Get() const { return m_Current; }
        SceneGraphNode* operator->() { return m_Current; }

    private:
        SceneGraphNode* m_Current;
        SceneGraphNode* m_Scope;
        std::stack<size_t> m_ChildIndices;
    };

public:

	SceneGraph() = default;

	// Replaces the current root node of the graph with the new one. Returns the old root.
	std::shared_ptr<st::gfx::SceneGraphNode> SetRoot(std::shared_ptr<SceneGraphNode> rootNode);

	// Attaches a node and its subgraph to the parent.
	// If the node is already attached to this or other graph, a deep copy of the subgraph is made first.
	std::shared_ptr<SceneGraphNode> Attach(std::shared_ptr<SceneGraphNode> parent, std::shared_ptr<SceneGraphNode> child);

    // Removes the node and its subgraph from the graph.
    std::shared_ptr<SceneGraphNode> Detach(const std::shared_ptr<SceneGraphNode>& node);

private:

    void RegisterLeaf(std::shared_ptr<SceneGraphLeaf> leaf);
    void UnregisterLeaf(std::shared_ptr<SceneGraphLeaf> leaf);

private:

	std::shared_ptr<SceneGraphNode> m_Root; // shared ownership
};

} // namespace st::gfx