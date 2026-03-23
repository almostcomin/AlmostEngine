#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraph.h"

alm::gfx::SceneGraphNode::SceneGraphNode() :
	m_HasBounds{},
	m_ContentFlags{ SceneContentFlags::None },
	m_DirtyFlags{ DirtyFlags::None },
	m_Name{ "<noname>" }
{
	m_WorldBounds.fill(alm::math::aabox3f::InitEmpty);
}

alm::gfx::SceneGraphNode::~SceneGraphNode()
{}

void alm::gfx::SceneGraphNode::SetLeaf(alm::unique<SceneGraphLeaf>&& leaf)
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

void alm::gfx::SceneGraphNode::OnLeafBoundsChanged()
{
	m_DirtyFlags |= DirtyFlags::Leaf;
	PropagateDirtyFlags(DirtyFlags::Subgraph);
}

alm::weak<alm::gfx::SceneGraphNode> alm::gfx::SceneGraphNode::GetChild(size_t idx) const
{
	return idx < m_Children.size() ? m_Children[idx].get_weak() : alm::weak<alm::gfx::SceneGraphNode>{};
}

alm::weak<alm::gfx::SceneGraphNode> alm::gfx::SceneGraphNode::GetParent() const
{
	return m_Parent;
}

void alm::gfx::SceneGraphNode::SetLocalTransform(const Transform& t)
{
	m_LocalTransform = t;
	m_DirtyFlags |= DirtyFlags::LocalTransform;
	PropagateDirtyFlags(DirtyFlags::Subgraph);
}

const float3& alm::gfx::SceneGraphNode::GetWorldPosition() const
{
	// Column major matrix
	return *(float3*)&m_WorldMatrix[3];
}

bool alm::gfx::SceneGraphNode::Test(BoundsType boundsType, std::span<const math::plane3f> planes) const
{
	assert(m_HasBounds[(int)boundsType] && "Test on a node without bounds!");
	for (const auto& plane : planes)
	{
		if (!m_WorldBounds[(int)boundsType].test(plane))
			return false;
	}
	return true;
}

void alm::gfx::SceneGraphNode::PropagateDirtyFlags(DirtyFlags flags)
{
    for (auto walker = SceneGraph::Walker(this); walker; walker.Up())
    {
        walker->m_DirtyFlags |= flags;
    }
}