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

	const std::shared_ptr<st::gfx::Mesh>& GetMesh() const { return m_Mesh; }

	void SetMeshSceneIndex(int i) { m_MeshSceneIndex = i; }
	int GetMeshSceneIndex() const { return m_MeshSceneIndex; }

	void SetMaterialSceneIndex(int i) { m_MaterialSceneIndex = i; }
	int GetMaterialSceneIndex() const { return m_MaterialSceneIndex; }

private:

	std::shared_ptr<st::gfx::Mesh> m_Mesh;
	int m_MeshSceneIndex;
	int m_MaterialSceneIndex;
};

} // namespace 