#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/Renderable.h"
#include "Gfx/Heightmap.h"

namespace alm::gfx
{

class SceneHeightmap : public SceneGraphLeaf, public IRenderable
{
public:

	SceneHeightmap();
	~SceneHeightmap();

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

	void SetPatchMeshGpuIndex(uint32_t variantIndex, uint32_t meshGpuIndex) { m_PatchMeshGpuIndices[variantIndex] = meshGpuIndex; }
	uint32_t GetPatchMeshGpuIndex(uint32_t variantIndex) const { return m_PatchMeshGpuIndices[variantIndex]; }

private:

	std::shared_ptr<Heightmap> m_Heightmap;
	std::array<uint32_t, Heightmap::kPatchMeshVariantsCount> m_PatchMeshGpuIndices;
};

} // namespace alm::gfx