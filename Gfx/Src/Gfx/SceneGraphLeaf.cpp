#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraphNode.h"

void alm::gfx::SceneGraphLeaf::OnBoundsChanged()
{
	if (m_Node)
	{
		m_Node->OnLeafBoundsChanged();
	}
}

