#pragma once
#include <vector>
#include <memory>
#include "Graphics/Transform.h"

namespace st::gfx
{
	class SceneGraph;
}

namespace st::gfx
{

class SceneGraphNode
{
public:

	void SetTransform(const Transform& t) { m_LocalTransform = t; }
	void SetName(const char* name) { m_Name = name; }
	void SetParent(std::weak_ptr<SceneGraphNode> parent) { m_Parent = parent; }
	void AddChild(std::shared_ptr<SceneGraphNode> node) { m_Children.push_back(node); }

private:

	std::weak_ptr<SceneGraph> m_Graph;
	std::weak_ptr<SceneGraphNode> m_Parent;
	std::vector<std::shared_ptr<SceneGraphNode>> m_Children;

	Transform m_LocalTransform;

	std::string m_Name;
};

};