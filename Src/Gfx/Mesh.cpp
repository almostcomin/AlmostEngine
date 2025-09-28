#include "Gfx/Mesh.h"

st::gfx::Mesh::Mesh(const char* name, const char* sourceFilename) :
	m_Name{ name ? name : "<null>" },
	m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" },
	m_Bounds{ st::math::aabox3f::InitEmpty }
{}

void st::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}