#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/SceneContentFlags.h"
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

	enum class Flags
	{
		None					= 0x00,
		CastShadows				= 0x01,
		ReceiveShadows			= 0x02,
		Visible					= 0x04,
		VisibleInReflections	= 0x08,
		Static					= 0x10,
		Default					= CastShadows | ReceiveShadows | Visible | VisibleInReflections
	};

public:

	MeshInstance(std::shared_ptr<Mesh> mesh);
	virtual ~MeshInstance();

	bool HasBounds() const override;
	const alm::math::aabox3f& GetBounds() const override;

	Flags GetInstanceFlags() const { return m_InstanceFlags; }
	void SetInstanceFlags(Flags flags);

	SceneContentFlags GetContentFlags() const override;
	Type GetType() const override { return Type::MeshInstance; }

	const std::shared_ptr<alm::gfx::Mesh>& GetMesh() const { return m_Mesh; }

	uint32_t GetMeshSceneIndex() const { return m_MeshSceneIndex; }
	uint32_t GetMaterialSceneIndex() const { return m_MaterialSceneIndex; }

	// IRenderable interface
	void CollectDrawInfos(const VisibleSetContext& context, std::vector<RenderableDrawInfo>& out) const override;

	//-- To be called by SceneGraph when registering the leaf

	void SetMeshSceneIndex(uint32_t i) { m_MeshSceneIndex = i; }
	void SetMaterialSceneIndex(uint32_t i) { m_MaterialSceneIndex = i; }

private:

	std::shared_ptr<alm::gfx::Mesh> m_Mesh;
	uint32_t m_MeshSceneIndex;
	uint32_t m_MaterialSceneIndex;
	Flags m_InstanceFlags;
};

ENUM_CLASS_FLAG_OPERATORS(gfx::MeshInstance::Flags)

} // namespace 
