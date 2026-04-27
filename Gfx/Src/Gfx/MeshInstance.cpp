#include "Gfx/GfxPCH.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"

alm::gfx::MeshInstance::MeshInstance(std::shared_ptr<alm::gfx::Mesh> mesh) :
	m_Mesh{ mesh },
	m_MeshSceneIndex{ -1 },
	m_MaterialSceneIndex{ -1 },
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

alm::rhi::CullMode alm::gfx::MeshInstance::GetCullMode() const
{
	return m_Mesh->GetMaterial()->GetCullMode();
}

alm::gfx::MaterialDomain alm::gfx::MeshInstance::GetMaterialDomain() const
{
	return m_Mesh->GetMaterial()->GetDomain();
}