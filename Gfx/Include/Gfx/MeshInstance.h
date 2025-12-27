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

	bool HasBounds() const override;
	const st::math::aabox3f& GetBounds() const override;

	SceneContentFlags GetContentFlags() const override { return SceneContentFlags::OpaqueMeshes; }
	Type GetType() const override { return Type::MeshInstance; }

	std::shared_ptr<st::gfx::Mesh> GetMesh() const { return m_Mesh; }

private:

	std::shared_ptr<st::gfx::Mesh> m_Mesh;
};

} // namespace 