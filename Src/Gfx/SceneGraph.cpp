#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"

st::gfx::SceneGraphNode* st::gfx::SceneGraph::Walker::Next()
{
    if (!m_Current) 
        return 0;

    if (m_Current->GetChildrenCount() > 0)
    {
        m_ChildIndices.push(0);
        m_Current = m_Current->GetChild(0);
        return m_Current;
    }

    while (m_Current != nullptr)
    {
        if (m_Current == m_Scope)
        {
            m_Current = nullptr;
            return nullptr;
        }

        if (!m_ChildIndices.empty())
        {
            size_t& siblingIdx = m_ChildIndices.top();
            ++siblingIdx;

            auto* parent = m_Current->GetParent();
            if (siblingIdx < parent->GetChildrenCount())
            {
                m_Current = parent->GetChild(siblingIdx);
                return m_Current;
            }

            m_ChildIndices.pop();
        }
        m_Current = m_Current->GetParent();
    }

    return nullptr;
}

st::gfx::SceneGraphNode* st::gfx::SceneGraph::Walker::Up()
{
    if (!m_Current)
        return 0;

    if (m_Current == m_Scope)
    {
        m_Current = nullptr;
        return nullptr;
    }

    if (!m_ChildIndices.empty())
        m_ChildIndices.pop();

    m_Current = m_Current->GetParent();
    return m_Current;
}

std::shared_ptr<st::gfx::SceneGraphNode> st::gfx::SceneGraph::SetRoot(std::shared_ptr<st::gfx::SceneGraphNode> rootNode)
{
    auto oldRoot = m_Root;
//    if (m_Root)
//        Detach(m_Root);

    Attach(nullptr, rootNode);

    return oldRoot;
}

std::shared_ptr<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Attach(
    std::shared_ptr<st::gfx::SceneGraphNode> parent, std::shared_ptr<st::gfx::SceneGraphNode> child)
{
    auto parentGraph = parent ? parent->m_Graph.lock() : shared_from_this();
    auto childGraph = child->m_Graph.lock();

    if (!parentGraph && !childGraph)
    {
        // operating on an orphaned subgraph - do not copy or register anything
        assert(parent);
        parent->m_Children.push_back(child);
        child->m_Parent = parent;
        return child;
    }

    assert(parentGraph.get() == this);
    std::shared_ptr<SceneGraphNode> attachedChild;

    if (childGraph)
    {
        // TODO
        assert(0);
    }
    else
    {
        // attaching a subgraph that has been detached from another graph (or never attached)

        for (auto walker = Walker{ child.get() }; walker; walker.Next())
        {
            walker->m_Graph = weak_from_this();
        }
        child->m_Parent = parent;

        if (parent)
        {
            parent->m_Children.push_back(child);
        }
        else
        {
            m_Root = child;
        }

        attachedChild = child;
    }

    attachedChild->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::SubgraphStructure);

    return attachedChild;
}

void st::gfx::SceneGraph::RegisterLeaf(std::shared_ptr<SceneGraphLeaf> leaf)
{
    // TODO
}

void st::gfx::SceneGraph::UnregisterLeaf(std::shared_ptr<SceneGraphLeaf> leaf)
{
    // TODO
}