#include "SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraph.h"

void st::gfx::SceneGraphNode::SetLeaf(std::shared_ptr<SceneGraphLeaf> leaf)
{ 
	auto graph = m_Graph.lock();

	if (m_Leaf)
	{
		m_Leaf->m_Node.reset();
		if (graph)
			graph->UnregisterLeaf(leaf);
	}

	m_Leaf = leaf; 
	m_Leaf->m_Node = weak_from_this();
	if (graph)
		graph->RegisterLeaf(leaf);

	m_Dirty |= DirtyFlags::Leaf;
	PropagateDirtyFlags(DirtyFlags::SubgraphStructure);
}

st::gfx::SceneGraphNode* st::gfx::SceneGraphNode::GetChild(size_t idx) const
{
	return idx < m_Children.size() ? m_Children[idx].get() : nullptr;
}

st::gfx::SceneGraphNode* st::gfx::SceneGraphNode::GetParent() const
{
	return m_Parent.lock().get();
}

void st::gfx::SceneGraphNode::PropagateDirtyFlags(DirtyFlags flags)
{
    for (auto walker = SceneGraph::Walker(this); walker; walker.Up())
    {
        walker->m_Dirty |= flags;
    }
}