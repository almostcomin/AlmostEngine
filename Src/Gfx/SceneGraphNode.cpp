#include "SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraph.h"

st::gfx::SceneGraphNode::SceneGraphNode()
{
}

st::gfx::SceneGraphNode::~SceneGraphNode()
{
}

void st::gfx::SceneGraphNode::SetLeaf(st::unique_with_weak_ptr<SceneGraphLeaf>&& leaf)
{ 
	if (m_Leaf)
	{
		m_Leaf->m_Node.reset();
		if (m_Graph)
			m_Graph->UnregisterLeaf(m_Leaf.get());
	}

	m_Leaf = std::move(leaf); 
	m_Leaf->m_Node = weak_from_this();
	if (m_Graph)
		m_Graph->RegisterLeaf(m_Leaf.get());

	m_Dirty |= DirtyFlags::Leaf;
	PropagateDirtyFlags(DirtyFlags::SubgraphStructure);
}

st::weak_handle<st::gfx::SceneGraphNode> st::gfx::SceneGraphNode::GetChild(size_t idx) const
{
	return idx < m_Children.size() ? m_Children[idx].weak() : st::weak_handle<st::gfx::SceneGraphNode>{};
}

st::weak_handle<st::gfx::SceneGraphNode> st::gfx::SceneGraphNode::GetParent() const
{
	return m_Parent;
}

void st::gfx::SceneGraphNode::PropagateDirtyFlags(DirtyFlags flags)
{
    for (auto walker = SceneGraph::Walker(this); walker; walker.Up())
    {
        walker->m_Dirty |= flags;
    }
}