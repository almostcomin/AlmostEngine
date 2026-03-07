#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneGraphNode.h"

void st::gfx::SceneGraphLeaf::OnBoundsChanged()
{
	if (m_Node)
	{
		m_Node->OnLeafBoundsChanged();
	}
}

