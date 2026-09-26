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

void alm::gfx::SceneGraphLeaf::SetVisible(bool b)
{
	if (IsVisible() == b)
		return;

	if (b)
	{
		m_RenderFlags |= SceneRenderFlags::Visible;
	}
	else
	{
		m_RenderFlags &= ~SceneRenderFlags::Visible;
	}

	// Re-serialize the GPU cull data (flags) of renderable leaves.
	// The BatchTable rebuild also excludes invisible instances (GpuSceneBuffers::RebuildBatchTable),
	// but until the cull data is re-uploaded the kernel would still scatter this instance.
	if (m_SceneIndex != UINT32_MAX && AsRenderable())
		OnContentChanged();

	// Showing an instance re-enters it into the BatchTable -> structural change
	// (hide stays O(1): the RF_VISIBLE kernel guard hides it without a rebuild)
	if (b && m_Node)
	{
		if (auto* graph = m_Node->m_Graph)
			graph->ReportBatchLayoutChanged();
	}
}

void alm::gfx::SceneGraphLeaf::SetCastShadows(bool b)
{
	if (CastShadows() == b)
		return;

	if (b)
	{
		m_RenderFlags |= SceneRenderFlags::CastShadows;
	}
	else
	{
		m_RenderFlags &= ~SceneRenderFlags::CastShadows;
	}

	// Re-serialize the GPU cull data (flags) of renderable leaves, same as SetVisible
	if (m_SceneIndex != UINT32_MAX && AsRenderable())
		OnContentChanged();
}

bool alm::gfx::SceneGraphLeaf::IsVisible() const
{
	return (m_RenderFlags & SceneRenderFlags::Visible) != 0;
}

bool alm::gfx::SceneGraphLeaf::CastShadows() const
{
	return (m_RenderFlags & SceneRenderFlags::CastShadows) != 0;
}