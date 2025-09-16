#pragma once
#include <memory>
#include "Graphics/SceneGraphNode.h"

namespace st::gfx
{

class SceneGraphNode;

class SceneGraph
{
public:

	SceneGraph() {}
	SceneGraph(std::shared_ptr<SceneGraphNode> root) : m_Root{ root } {}

private:

	std::shared_ptr<SceneGraphNode> m_Root; // shared ownership
};

} // namespace st::gfx