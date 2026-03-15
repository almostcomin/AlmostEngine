#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"

st::gfx::MeshInstance::MeshInstance(std::shared_ptr<st::gfx::Mesh> mesh) :
	m_Mesh{ mesh },
	m_MeshSceneIndex{ -1 },
	m_MaterialSceneIndex{ -1 }
{}

st::gfx::MeshInstance::~MeshInstance()
{}

bool st::gfx::MeshInstance::HasBounds() const
{
	return m_Mesh ? true : false;
}

const st::math::aabox3f& st::gfx::MeshInstance::GetBounds() const
{
	return m_Mesh ? m_Mesh->GetBounds() : st::math::aabox3f::get_empty();
}

st::rhi::CullMode st::gfx::MeshInstance::GetCullMode() const
{
	return m_Mesh->GetMaterial()->GetCullMode();
}

st::gfx::MaterialDomain st::gfx::MeshInstance::GetMaterialDomain() const
{
	return m_Mesh->GetMaterial()->GetDomain();
}