#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneContentFlags.h"

namespace st::gfx
{
	class SceneGraphNode;
	class Mesh;
}


namespace st::gfx
{

class MeshInstance : public SceneGraphLeaf
{
public:

	MeshInstance(std::shared_ptr<Mesh> mesh);
	virtual ~MeshInstance();

	const st::math::aabox3f& GetLocalBBox() const override;
	SceneContentFlags GetContentFlags() const override { return SceneContentFlags::OpaqueMeshes; }

private:

	std::shared_ptr<st::gfx::Mesh> m_Mesh;
};

} // namespace 