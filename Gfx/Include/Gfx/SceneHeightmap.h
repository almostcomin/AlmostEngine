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
	const std::shared_ptr<Heightmap>& GetHeightmap() const { return m_Heightmap; }

	// SceneGraphLeaf interface
	bool HasBounds() const override { return m_Heightmap ? true : false; }
	const aabox3f& GetBounds() const override;
	SceneContentFlags GetContentFlags() const override { return alm::gfx::SceneContentFlags::Meshes | alm::gfx::SceneContentFlags::ShadowCasters; }
	Type GetType() const override { return SceneGraphLeaf::Type::Heightmap; }

	IRenderable* AsRenderable() override { return this; }
	const IRenderable* AsRenderable() const override { return this; }

	// IRenderable interface
	void CollectDrawInfos(const VisibleSetContext& context, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const override;

	void SetPatchMeshGpuIndex(uint32_t idx) { m_PatchMeshGpuIndex = idx; }
	uint32_t GetPatchMeshGpuIndex() const { return m_PatchMeshGpuIndex; }

private:

	std::shared_ptr<Heightmap> m_Heightmap;
	uint32_t m_PatchMeshGpuIndex = UINT32_MAX;
};

} // namespace alm::gfx