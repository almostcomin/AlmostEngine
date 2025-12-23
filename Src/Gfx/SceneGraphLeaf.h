#pragma once

#include "Core/Util.h"
#include "Core/Memory.h"
#include "Core/Math/aabox.h"
#include "Gfx/SceneContentFlags.h"

namespace st::gfx
{
	class SceneGraphNode;
}

namespace st::gfx
{

class SceneGraphLeaf : private st::noncopyable_nonmovable
{
	friend class SceneGraphNode;

public:

	enum class Type
	{
		MeshInstance,
		Camera
	};

	SceneGraphLeaf() = default;
	virtual ~SceneGraphLeaf() {}

	virtual bool HasBounds() const = 0;
	virtual const st::math::aabox3f& GetBounds() const { return st::math::aabox3f::empty(); }
	virtual SceneContentFlags GetContentFlags() const { return st::gfx::SceneContentFlags::None; }
	virtual Type GetType() const = 0;

	st::weak<SceneGraphNode> GetNode() const { return m_Node; }

private:

	st::weak<SceneGraphNode> m_Node;
};

};