#include "Gfx/Mesh.h"

void st::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}