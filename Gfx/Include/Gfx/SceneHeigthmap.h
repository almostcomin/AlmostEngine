#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/Renderable.h"

namespace alm::gfx
{

class Heightmap;

class SceneHeightmap : public SceneGraphLeaf, public IRenderable
{
public:

	void SetHeightmap(std::shared_ptr<Heightmap> heightmap) { m_Heightmap = heightmap; }

	// SceneGraphLeaf interface
	bool HasBounds() const override { return m_Heightmap ? true : false; }
	const alm::math::aabox3f& GetBounds() const override;
	SceneContentFlags GetContentFlags() const override { return alm::gfx::SceneContentFlags::Meshes; }
	Type GetType() const override { return SceneGraphLeaf::Type::Heightmap; }

	// IRenderable interface
	void CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const override;

private:

	std::shared_ptr<Heightmap> m_Heightmap;
};

} // namespace alm::gfx