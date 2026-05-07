#include "Gfx/GfxPCH.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"

alm::gfx::MeshInstance::MeshInstance(std::shared_ptr<alm::gfx::Mesh> mesh) :
	m_Mesh{ mesh },
	m_MeshSceneIndex{ UINT32_MAX },
	m_MaterialSceneIndex{ UINT32_MAX },
	m_InstanceFlags{ Flags::Default }
{}

alm::gfx::MeshInstance::~MeshInstance()
{}

bool alm::gfx::MeshInstance::HasBounds() const
{
	return m_Mesh ? true : false;
}

const alm::math::aabox3f& alm::gfx::MeshInstance::GetBounds() const
{
	return m_Mesh ? m_Mesh->GetBounds() : alm::math::aabox3f::get_empty();
}

void alm::gfx::MeshInstance::SetInstanceFlags(Flags flags)
{
	m_InstanceFlags = flags;
}

alm::gfx::SceneContentFlags alm::gfx::MeshInstance::GetContentFlags() const
{ 
	SceneContentFlags flags = SceneContentFlags::Meshes;

	if (has_any_flag(m_InstanceFlags, Flags::CastShadows))
		flags |= SceneContentFlags::ShadowCasters;

	return flags;
}

alm::gfx::MaterialDomain alm::gfx::MeshInstance::GetMaterialDomain() const
{
	return m_Mesh->GetMaterial()->GetDomain();
}

alm::rhi::CullMode alm::gfx::MeshInstance::GetCullMode() const
{
	return m_Mesh->GetMaterial()->GetCullMode();
}

uintptr_t alm::gfx::MeshInstance::GetBatchKey() const
{
	return (uintptr_t)(m_Mesh.get());
}

alm::gfx::RenderableDrawInfo alm::gfx::MeshInstance::GetDrawInfo() const
{
	return RenderableDrawInfo{
		.InstanceIdx = GetLeafSceneIndex(),
		.MeshIndex = m_MeshSceneIndex,
		.MaterialIndex = m_MaterialSceneIndex,
		.IndexCount = m_Mesh ? m_Mesh->GetIndexCount() : 0
	};
}