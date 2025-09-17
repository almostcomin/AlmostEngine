#pragma once

#include <memory>
#include "Core/Util.h"
#include "Gfx/Math/aabox.h"

namespace st::gfx
{
	class SceneGraphNode;
	enum class SceneContentFlags : uint32_t;
}

namespace st::gfx
{

class SceneGraphLeaf : private st::noncopyable_nonmovable
{
public:

	virtual ~SceneGraphLeaf() {}

	virtual const st::math::aabox3f& GetLocalBBox() const = 0;
	virtual SceneContentFlags GetContentFlags() const = 0;

private:

	std::weak_ptr<SceneGraphNode> m_Node;
};

};