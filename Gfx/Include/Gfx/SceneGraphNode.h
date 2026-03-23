#pragma once
#include <vector>
#include <string>
#include "Gfx/Transform.h"
#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/aabox.h"
#include "Core/Math.h"
#include "Gfx/SceneContentFlags.h"
#include "Gfx/SceneBounds.h"

namespace alm::gfx
{
	class SceneGraph;
	class SceneGraphLeaf;
}

namespace alm::gfx
{

class SceneGraphNode : public alm::enable_weak_from_this<SceneGraphNode>, private alm::noncopyable_nonmovable
{
	friend class SceneGraph;

public:

	enum class DirtyFlags : uint32_t
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

	void SetLeaf(alm::unique<SceneGraphLeaf>&& leaf);
	void OnLeafBoundsChanged();

	void SetLocalTransform(const Transform& t);

	size_t GetChildrenCount() const { return m_Children.size(); }
	alm::weak<alm::gfx::SceneGraphNode> GetChild(size_t idx) const;
	alm::weak<alm::gfx::SceneGraphNode> GetParent() const;

	bool HasLeaf() const { return m_Leaf; }
	alm::weak<SceneGraphLeaf> GetLeaf() const { return m_Leaf.get_weak(); }

	const Transform& GetLocalTransform() const { return m_LocalTransform; }
	const float4x4& GetWorldTransform() const { return m_WorldMatrix; }
	const float3& GetWorldPosition() const;

	bool HasBounds(BoundsType type) const { return m_HasBounds[(int)type]; }
	const alm::math::aabox3f& GetWorldBounds(BoundsType type) const { return m_WorldBounds[(int)type]; }

	bool Test(BoundsType boundsType, std::span<const math::plane3f> planes) const;

	SceneContentFlags GetContentFlags() const { return m_ContentFlags; }

	const std::string& GetName() const { return m_Name; }

private:

	void PropagateDirtyFlags(DirtyFlags flags);

private:

	alm::weak<SceneGraph> m_Graph;
	alm::weak<SceneGraphNode> m_Parent;
	std::vector<alm::unique<SceneGraphNode>> m_Children;
	alm::unique<SceneGraphLeaf> m_Leaf;

	Transform m_LocalTransform;
	float4x4 m_WorldMatrix;

	std::array<bool, (int)BoundsType::_Size> m_HasBounds;
	std::array<alm::math::aabox3f, (int)BoundsType::_Size> m_WorldBounds;

	SceneContentFlags m_ContentFlags; // This leaf content flags plus children content flags

	DirtyFlags m_DirtyFlags;
	std::string m_Name;
};

ENUM_CLASS_FLAG_OPERATORS(SceneGraphNode::DirtyFlags);

} // namespace st::gfx