#include "Gfx/GfxPCH.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraph.h"

alm::gfx::SceneGraphNode::SceneGraphNode(const std::string& name) :
	m_ContentFlags{ SceneContentFlags::None },
	m_DirtyFlags{ DirtyFlags::None },
	m_Name{ name.empty() ? "<noname>" : name }
{
	m_WorldBounds.fill(alm::math::aabox3f::InitEmpty);
}

alm::gfx::SceneGraphNode::~SceneGraphNode()
{}

void alm::gfx::SceneGraphNode::AddChild(alm::unique<SceneGraphNode>&& child)
{
	assert(!child->m_Graph);
	assert(!child->m_Parent);

	SceneGraphNode* pchild = child.get();
	m_Children.push_back(std::move(child));

	pchild->m_Parent = weak_from_this();
	if (m_Graph)
	{
		m_Graph->OnNodeAttached(pchild);
	}
}

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

const alm::math::aabox3f& alm::gfx::SceneGraphNode::GetWorldBounds(SceneContentType type) const
{ 
	assert(HasBoundsCategory(type));
	return m_WorldBounds[(int)type]; 
}

bool alm::gfx::SceneGraphNode::Test(SceneContentType boundsType, std::span<const math::plane3f> planes) const
{
	assert(HasBoundsCategory(boundsType));
	assert(m_WorldBounds[(int)boundsType].valid());

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