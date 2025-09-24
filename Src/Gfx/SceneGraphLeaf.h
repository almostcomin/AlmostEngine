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

	SceneGraphLeaf() = default;
	virtual ~SceneGraphLeaf() {}

	virtual const st::math::aabox3f& GetLocalBBox() const { return st::math::aabox3f::Empty(); }
	virtual SceneContentFlags GetContentFlags() const { return st::gfx::SceneContentFlags::None; }

private:

	st::weak_handle<SceneGraphNode> m_Node;
};

};