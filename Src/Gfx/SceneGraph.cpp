#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"

st::weak<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Walker::Next()
{
    if (!m_Current) 
        return {};

    if (m_Current->GetChildrenCount() > 0)
    {
        m_ChildIndices.push(0);
        m_Current = m_Current->GetChild(0);
        return m_Current;
    }

    while (m_Current)
    {
        if (m_Current == m_Scope)
        {
            m_Current.reset();
            return {};
        }

        if (!m_ChildIndices.empty())
        {
            size_t& siblingIdx = m_ChildIndices.top();
            ++siblingIdx;

            auto parent = m_Current->GetParent();
            if (siblingIdx < parent->GetChildrenCount())
            {
                m_Current = parent->GetChild(siblingIdx);
                return m_Current;
            }

            m_ChildIndices.pop();
        }
        m_Current = m_Current->GetParent();
    }

    return {};
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
    auto childGraph = child.get_weak();

    if (!parentGraph && !childGraph)
    {
        // operating on an orphaned subgraph - do not copy or register anything
        assert(parent);
        parent->m_Children.push_back(std::move(child));
        child->m_Parent = parent->weak_from_this();
        return child.get_weak();
    }

    assert(parentGraph.get() == this);
    st::weak<SceneGraphNode> attachedChild;

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
        child->m_Parent = parent->weak_from_this();

        if (parent)
        {
            parent->m_Children.push_back(std::move(child));
        }
        else
        {
            m_Root = std::move(child);
        }

        attachedChild = child.get_weak();
    }

    attachedChild->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::SubgraphStructure);
    return attachedChild;
}

st::unique<st::gfx::SceneGraphNode> st::gfx::SceneGraph::Detach(
    const SceneGraphNode* node)
{
    // TODO
    return {};
}

void st::gfx::SceneGraph::RegisterLeaf(SceneGraphLeaf* leaf)
{
    // TODO
}

void st::gfx::SceneGraph::UnregisterLeaf(SceneGraphLeaf* leaf)
{
    // TODO
}