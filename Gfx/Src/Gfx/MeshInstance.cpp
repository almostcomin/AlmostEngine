#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"

alm::gfx::MeshInstance::MeshInstance(std::shared_ptr<alm::gfx::Mesh> mesh) :
	m_Mesh{ mesh },
	m_MeshSceneIndex{ -1 },
	m_MaterialSceneIndex{ -1 }
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

alm::rhi::CullMode alm::gfx::MeshInstance::GetCullMode() const
{
	return m_Mesh->GetMaterial()->GetCullMode();
}

alm::gfx::MaterialDomain alm::gfx::MeshInstance::GetMaterialDomain() const
{
	return m_Mesh->GetMaterial()->GetDomain();
}