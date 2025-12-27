#pragma once
#include <vector>
#include <string>
#include "Gfx/Transform.h"
#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/aabox.h"
#include "Core/Math.h"
#include "Gfx/SceneContentFlags.h"

namespace st::gfx
{
	class SceneGraph;
	class SceneGraphLeaf;
}

namespace st::gfx
{

class SceneGraphNode : public st::enable_weak_from_this<SceneGraphNode>, private st::noncopyable_nonmovable
{
	friend class SceneGraph;

public:

	enum struct DirtyFlags : uint32_t
	{
		None = 0,
		LocalTransform = 0x01,	// Local transoform has changed
		Leaf = 0x02,			// Leaf has changed
		Subgraph = 0x04,		// Subgraph has changed (local transform, leaf attached i.e.)

		All = (LocalTransform | Leaf | Subgraph)
	};

public:

	SceneGraphNode();
	~SceneGraphNode();

	void SetName(const char* name) { m_Name = name; }

	void SetLeaf(st::unique<SceneGraphLeaf>&& leaf);

	void SetLocalTransform(const Transform& t);

	size_t GetChildrenCount() const { return m_Children.size(); }
	st::weak<st::gfx::SceneGraphNode> GetChild(size_t idx) const;
	st::weak<st::gfx::SceneGraphNode> GetParent() const;

	bool HasLeaf() const { return m_Leaf; }
	st::weak<SceneGraphLeaf> GetLeaf() const { return m_Leaf.get_weak(); }

	const Transform& GetLocalTransform() const { return m_LocalTransform; }
	const float4x4& GetWorldTransform() const { return m_WorldMatrix; }

	bool HasBounds() const { return m_HasBounds; }
	const st::math::aabox3f& GetWorldBounds() { return m_WorldBounds; }

	SceneContentFlags GetContentFlags() const { return m_ContentFlags; }

	const std::string& GetName() const { return m_Name; }

private:

	void PropagateDirtyFlags(DirtyFlags flags);

private:

	st::weak<SceneGraph> m_Graph;
	st::weak<SceneGraphNode> m_Parent;
	std::vector<st::unique<SceneGraphNode>> m_Children;
	st::unique<SceneGraphLeaf> m_Leaf;

	Transform m_LocalTransform;
	float4x4 m_WorldMatrix;

	bool m_HasBounds;
	st::math::aabox3f m_WorldBounds;

	SceneContentFlags m_ContentFlags; // This includes leaf content flags push children

	DirtyFlags m_DirtyFlags;
	std::string m_Name;
};

ENUM_CLASS_FLAG_OPERATORS(SceneGraphNode::DirtyFlags);
/*
inline SceneGraphNode::DirtyFlags operator | (SceneGraphNode::DirtyFlags a, SceneGraphNode::DirtyFlags b) { return SceneGraphNode::DirtyFlags(uint32_t(a) | uint32_t(b)); }
inline SceneGraphNode::DirtyFlags operator & (SceneGraphNode::DirtyFlags a, SceneGraphNode::DirtyFlags b) { return SceneGraphNode::DirtyFlags(uint32_t(a) & uint32_t(b)); }
inline SceneGraphNode::DirtyFlags operator ~ (SceneGraphNode::DirtyFlags a) { return SceneGraphNode::DirtyFlags(~uint32_t(a)); }
inline SceneGraphNode::DirtyFlags operator |= (SceneGraphNode::DirtyFlags& a, SceneGraphNode::DirtyFlags b) { a = SceneGraphNode::DirtyFlags(uint32_t(a) | uint32_t(b)); return a; }
inline SceneGraphNode::DirtyFlags operator &= (SceneGraphNode::DirtyFlags& a, SceneGraphNode::DirtyFlags b) { a = SceneGraphNode::DirtyFlags(uint32_t(a) & uint32_t(b)); return a; }
inline bool operator !(SceneGraphNode::DirtyFlags a) { return uint32_t(a) == 0; }
inline bool operator ==(SceneGraphNode::DirtyFlags a, uint32_t b) { return uint32_t(a) == b; }
inline bool operator !=(SceneGraphNode::DirtyFlags a, uint32_t b) { return uint32_t(a) != b; }
inline bool any(SceneGraphNode::DirtyFlags a) { return a != 0; }
*/
} // namespace st::gfx