#include "Gfx/Material.h"

st::gfx::Material::Material(const std::string& name, const std::string& filename) :
	m_Name{ name },
	m_SourceFileName{ filename }
{}

st::gfx::Material::~Material()
{}

void st::gfx::Material::SetDiffuseTexture(rapi::TextureHandle textureHandle)
{
	m_DiffuseTexture = textureHandle;
}