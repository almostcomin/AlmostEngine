#pragma once

#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/aabox.h"
#include "Gfx/SceneContentFlags.h"

namespace alm::gfx
{
	class SceneGraphNode;
}

namespace alm::gfx
{

class SceneGraphLeaf : private alm::noncopyable_nonmovable
{
	friend class SceneGraphNode;
	friend class SceneGraph;

public:

	enum class Type
	{
		MeshInstance,
		Camera,
		DirectionalLight,
		PointLight,
		SpotLight
	};

	SceneGraphLeaf() = default;
	virtual ~SceneGraphLeaf() {}

	virtual bool HasBounds() const { return false; }
	virtual const alm::math::aabox3f& GetBounds() const { return alm::math::aabox3f::get_empty(); }
	virtual SceneContentFlags GetContentFlags() const { return alm::gfx::SceneContentFlags::None; }
	virtual Type GetType() const = 0;

	void OnBoundsChanged();

	alm::weak<SceneGraphNode> GetNode() const { return m_Node; }
	int GetLeafSceneIndex() const { return m_SceneIndex; }

private:

	alm::weak<SceneGraphNode> m_Node;
	int m_SceneIndex = -1;
};

};