#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneFlags.h"
#include "Gfx/MaterialDomain.h"
#include "Gfx/Renderable.h"
#include "RHI/RasterizerState.h"

namespace alm::gfx
{
	class SceneGraphNode;
	class Mesh;
}


namespace alm::gfx
{

class MeshInstance : public SceneGraphLeaf, public IRenderable
{
public:

	MeshInstance(std::shared_ptr<Mesh> mesh);
	virtual ~MeshInstance();

	bool HasBounds() const override;
	const alm::aabox3f& GetBounds() const override;

	SceneContentFlags GetContentFlags() const override;
	Type GetType() const override { return Type::MeshInstance; }

	IRenderable* AsRenderable() override { return this; }
	const IRenderable* AsRenderable() const override { return this; }

	const std::shared_ptr<alm::gfx::Mesh>& GetMesh() const { return m_Mesh; }

	uint32_t GetMeshSceneIndex() const { return m_MeshSceneIndex; }

	// IRenderable interface
	void CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const override;

	//-- To be called by GpuSceneBuffers when registering the leaf
	void SetMeshSceneIndex(uint32_t i) { m_MeshSceneIndex = i; }

private:

	std::shared_ptr<alm::gfx::Mesh> m_Mesh;
	uint32_t m_MeshSceneIndex;
};

} // namespace 
