#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraph.h"

st::gfx::SceneGraphNode::SceneGraphNode() :
	m_HasBounds{ false },
	m_WorldBounds{ st::math::aabox3f::InitEmpty },
	m_ContentFlags{ SceneContentFlags::None },
	m_DirtyFlags{ DirtyFlags::None },
	m_Name{ "noname" }
{}

st::gfx::SceneGraphNode::~SceneGraphNode()
{}

void st::gfx::SceneGraphNode::SetLeaf(st::unique<SceneGraphLeaf>&& leaf)
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

	m_DirtyFlags |= DirtyFlags::Leaf;
	PropagateDirtyFlags(DirtyFlags::Subgraph);
}

st::weak<st::gfx::SceneGraphNode> st::gfx::SceneGraphNode::GetChild(size_t idx) const
{
	return idx < m_Children.size() ? m_Children[idx].get_weak() : st::weak<st::gfx::SceneGraphNode>{};
}

st::weak<st::gfx::SceneGraphNode> st::gfx::SceneGraphNode::GetParent() const
{
	return m_Parent;
}

void st::gfx::SceneGraphNode::SetLocalTransform(const Transform& t)
{
	m_LocalTransform = t;
	m_DirtyFlags |= DirtyFlags::LocalTransform;
	PropagateDirtyFlags(DirtyFlags::Subgraph);
}

void st::gfx::SceneGraphNode::PropagateDirtyFlags(DirtyFlags flags)
{
    for (auto walker = SceneGraph::Walker(this); walker; walker.Up())
    {
        walker->m_DirtyFlags |= flags;
    }
}