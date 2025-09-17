#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"

st::gfx::MeshInstance::MeshInstance(std::shared_ptr<st::gfx::Mesh> mesh) : m_Mesh{ mesh }
{
}

st::gfx::MeshInstance::~MeshInstance()
{
}

const st::math::aabox3f& st::gfx::MeshInstance::GetLocalBBox() const
{
	return m_Mesh ? m_Mesh->GetAABBox() : st::math::aabox3f::Empty();
}

