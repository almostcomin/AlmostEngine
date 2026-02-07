#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"

st::gfx::MeshInstance::MeshInstance(std::shared_ptr<st::gfx::Mesh> mesh) :
	m_Mesh{ mesh }
{
}

st::gfx::MeshInstance::~MeshInstance()
{
}

bool st::gfx::MeshInstance::HasBounds() const
{
	return m_Mesh ? true : false;
}

const st::math::aabox3f& st::gfx::MeshInstance::GetBounds() const
{
	return m_Mesh ? m_Mesh->GetBounds() : st::math::aabox3f::get_empty();
}