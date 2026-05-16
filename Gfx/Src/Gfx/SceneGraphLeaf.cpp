#include "Gfx/GfxPCH.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraph.h"

void alm::gfx::SceneGraphLeaf::OnBoundsChanged()
{
	auto* node = m_Node.get();
	if (!node)
		return;

	// We have to update the bounds of the parents
	node->m_DirtyFlags |= SceneGraphNode::DirtyFlags::Leaf;
	node->PropagateDirtyFlags(SceneGraphNode::DirtyFlags::Subgraph);
}

void alm::gfx::SceneGraphLeaf::OnContentChanged()
{
	auto node = m_Node.get();
	if (!node || !node->m_Graph)
		return;

	node->m_Graph->ReportLeafDirty(this);
}

const float4x4& alm::gfx::SceneGraphLeaf::GetWorldTransform() const
{
	return m_Node ? m_Node->GetWorldTransform() : float4x4_I;
}