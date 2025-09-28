#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"

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
    attachedChild->m_DirtyFlags |= (SceneGraphNode::DirtyFlags::LocalTransform | SceneGraphNode::DirtyFlags::Leaf);
    attachedChild->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);

    return attachedChild;
}

st::unique<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Detach(
    const SceneGraphNode* node)
{
    // TODO
    return {};
}

void st::gfx::SceneGraph::Refresh()
{
    // Root dirty?
    if (any(m_Root->m_DirtyFlags & SceneGraphNode::DirtyFlags::Subgraph))
    {
        Walker walker{ m_Root.get_weak() };
        while (walker)
        {
            if (any(walker->m_DirtyFlags & (SceneGraphNode::DirtyFlags::LocalTransform | SceneGraphNode::DirtyFlags::Leaf)))
            {
                // --- Subgraph needs update

                Walker subgraphWalker{ *walker };
                while (subgraphWalker)
                {
                    auto node = *subgraphWalker;

                    // Update world transform
                    if (node->m_Parent)
                    {
                        node->m_WorldMatrix = node->m_LocalTransform.GetMatrix() * node->m_Parent->m_WorldMatrix;
                    }
                    else
                    {
                        node->m_WorldMatrix = node->m_LocalTransform.GetMatrix();
                    }
                    node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::LocalTransform;

                    // Reset world bounds using leaf data
                    if (node->m_Leaf && node->m_Leaf->HasBounds())
                    {
                        node->m_WorldBounds = node->m_Leaf->GetBounds() * node->m_WorldMatrix;
                        node->m_HasBounds = true;
                    }
                    else
                    {
                        node->m_HasBounds = false;
                    }
                    node->m_DirtyFlags &= ~SceneGraphNode::DirtyFlags::Leaf;

                    int depth = subgraphWalker.Next();

                    // Update parent bounds if next is sibling or parent
                    if (node->m_HasBounds && depth <= 0) // Sibling or going up.
                    {
                        for (int i = depth; i <= 0; ++i)
                        {
                            if (!node->m_Parent)
                                break;

                            assert(node->m_Parent);
                            // Update parent bounds
                            node->m_Parent->m_WorldBounds.merge(node->m_WorldBounds);
                            node->m_Parent->m_HasBounds = true;

                            node = node->m_Parent;
                        }
                    }
                } // Subgraph loop

                // Since we have updated a sub-graph, we need to update parent bounds
                if (walker->m_HasBounds)
                {
                    auto node = *walker;
                    while (node->m_Parent)
                    {
                        node->m_Parent->m_WorldBounds.merge(node->m_WorldBounds);
                        node->m_Parent->m_HasBounds = true;
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
    // TODO
}

void st::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{
    // TODO
}