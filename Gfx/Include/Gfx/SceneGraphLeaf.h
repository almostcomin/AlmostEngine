#pragma once

#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/aabox.h"
#include "Gfx/SceneFlags.h"

namespace alm::gfx
{
	class SceneGraphNode;
	class IRenderable;
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
		SpotLight,
		Heightmap,

		_Size
	};

	SceneGraphLeaf() = default;
	virtual ~SceneGraphLeaf() {}

	virtual bool HasBounds() const { return false; }
	virtual const alm::aabox3f& GetBounds() const { return alm::aabox3f::get_empty(); }
	virtual SceneContentFlags GetContentFlags() const { return alm::gfx::SceneContentFlags::None; }
	virtual Type GetType() const = 0;

	virtual IRenderable* AsRenderable() { return nullptr; }
	virtual const IRenderable* AsRenderable() const { return nullptr; }

	SceneRenderFlags GetRenderFlags() const { return m_RenderFlags; }

	void OnBoundsChanged();
	void OnContentChanged();

	void SetLeafSceneIndex(uint32_t idx) { m_SceneIndex = idx; }

	alm::weak<SceneGraphNode> GetNode() const { return m_Node; }
	const float4x4& GetWorldTransform() const;

	uint32_t GetLeafSceneIndex() const { return m_SceneIndex; }

	void SetVisible(bool b);

protected:

	SceneRenderFlags m_RenderFlags = SceneRenderFlags::None;

private:

	alm::weak<SceneGraphNode> m_Node;
	uint32_t m_SceneIndex = UINT32_MAX;
};

};