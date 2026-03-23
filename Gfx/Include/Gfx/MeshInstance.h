#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneContentFlags.h"
#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"

namespace alm::gfx
{
	class SceneGraphNode;
	class Mesh;
}


namespace alm::gfx
{

class MeshInstance : public SceneGraphLeaf
{
public:

	MeshInstance(std::shared_ptr<Mesh> mesh);
	virtual ~MeshInstance();

	bool HasBounds() const override;
	BoundsType GetBoundsType() const override { return BoundsType::Mesh; }
	const alm::math::aabox3f& GetBounds() const override;

	SceneContentFlags GetContentFlags() const override { return SceneContentFlags::OpaqueMeshes; }
	Type GetType() const override { return Type::MeshInstance; }

	const std::shared_ptr<alm::gfx::Mesh>& GetMesh() const { return m_Mesh; }

	void SetMeshSceneIndex(int i) { m_MeshSceneIndex = i; }
	int GetMeshSceneIndex() const { return m_MeshSceneIndex; }

	void SetMaterialSceneIndex(int i) { m_MaterialSceneIndex = i; }
	int GetMaterialSceneIndex() const { return m_MaterialSceneIndex; }

	rhi::CullMode GetCullMode() const;
	MaterialDomain GetMaterialDomain() const;

private:

	std::shared_ptr<alm::gfx::Mesh> m_Mesh;
	int m_MeshSceneIndex;
	int m_MaterialSceneIndex;
};

} // namespace 