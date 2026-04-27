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

	int GetMeshSceneIndex() const { return m_MeshSceneIndex; }
	int GetMaterialSceneIndex() const { return m_MaterialSceneIndex; }

	rhi::CullMode GetCullMode() const;
	MaterialDomain GetMaterialDomain() const;

	//-- To be called by SceneGraph when registering the leaf

	void SetMeshSceneIndex(int i) { m_MeshSceneIndex = i; }
	void SetMaterialSceneIndex(int i) { m_MaterialSceneIndex = i; }

private:

	std::shared_ptr<alm::gfx::Mesh> m_Mesh;
	int m_MeshSceneIndex;
	int m_MaterialSceneIndex;
	Flags m_InstanceFlags;
};

ENUM_CLASS_FLAG_OPERATORS(gfx::MeshInstance::Flags)

} // namespace 
