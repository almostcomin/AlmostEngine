#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Gfx/Transform.h"
#include "Core/Util.h"

namespace st::gfx
{
	class SceneGraph;
	class SceneGraphLeaf;
}

namespace st::gfx
{

class SceneGraphNode : public std::enable_shared_from_this<SceneGraphNode>, private st::noncopyable_nonmovable
{
	friend class SceneGraph;

public:

	enum struct DirtyFlags : uint32_t
	{
		None = 0,
		WorldTransform = 0x01,
		SubgraphStructure = 0x02,
		Leaf = 0x04
	};

public:

	SceneGraphNode() = default;

	void SetTransform(const Transform& t) { m_LocalTransform = t; }
	void SetName(const char* name) { m_Name = name; }

	size_t GetChildrenCount() const { return m_Children.size(); }
	st::gfx::SceneGraphNode* GetChild(size_t idx) const;
	st::gfx::SceneGraphNode* GetParent() const;

	void SetLeaf(std::shared_ptr<SceneGraphLeaf> leaf);

private:

	void PropagateDirtyFlags(DirtyFlags flags);

private:

	std::weak_ptr<SceneGraph> m_Graph;
	std::weak_ptr<SceneGraphNode> m_Parent;
	std::vector<std::shared_ptr<SceneGraphNode>> m_Children;
	std::shared_ptr<SceneGraphLeaf> m_Leaf;

	Transform m_LocalTransform;
	glm::mat4x4 m_WorldMatrix;

	DirtyFlags m_Dirty;

	std::string m_Name;
};

inline SceneGraphNode::DirtyFlags operator | (SceneGraphNode::DirtyFlags a, SceneGraphNode::DirtyFlags b) { return SceneGraphNode::DirtyFlags(uint32_t(a) | uint32_t(b)); }
inline SceneGraphNode::DirtyFlags operator & (SceneGraphNode::DirtyFlags a, SceneGraphNode::DirtyFlags b) { return SceneGraphNode::DirtyFlags(uint32_t(a) & uint32_t(b)); }
inline SceneGraphNode::DirtyFlags operator ~ (SceneGraphNode::DirtyFlags a) { return SceneGraphNode::DirtyFlags(~uint32_t(a)); }
inline SceneGraphNode::DirtyFlags operator |= (SceneGraphNode::DirtyFlags& a, SceneGraphNode::DirtyFlags b) { a = SceneGraphNode::DirtyFlags(uint32_t(a) | uint32_t(b)); return a; }
inline SceneGraphNode::DirtyFlags operator &= (SceneGraphNode::DirtyFlags& a, SceneGraphNode::DirtyFlags b) { a = SceneGraphNode::DirtyFlags(uint32_t(a) & uint32_t(b)); return a; }
inline bool operator !(SceneGraphNode::DirtyFlags a) { return uint32_t(a) == 0; }
inline bool operator ==(SceneGraphNode::DirtyFlags a, uint32_t b) { return uint32_t(a) == b; }
inline bool operator !=(SceneGraphNode::DirtyFlags a, uint32_t b) { return uint32_t(a) != b; }

} // namespace st::gfx